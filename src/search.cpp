#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "corner.hpp"
#include "cube.hpp"
#include "edge.hpp"
#include "logger.hpp"
#include "rotation.hpp"
#include "transposition_table.hpp"
#include "random_position.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"
#include "thread_pool.hpp"


#ifdef USE_CUDA
#include "search_bridge.hpp"
#endif  // USE_CUDA


using Frontier = std::vector<std::vector<std::vector<std::pair<State, uint8_t>>>>;
constexpr int kNumHeuristicLayers = 60;

std::map<State, uint8_t> upload_to_leaf_map;


void SolutionTB(std::vector<Rotations>& tb_rotations, int tb_layer, State state) {
    for (int layer = tb_layer - 1; layer >= 0; layer--) {
        uint64_t corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
        for (uint8_t rotation = 0; rotation < kNumRot; rotation++) {
            if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
                continue;
            }
            State next_state = state;
            corner::Rotate(next_state.corner_pos, next_state.corner_orient, rotation);
            edge::Rotate(next_state.edge_pos, next_state.edge_sym, next_state.edge_orient, rotation);
            if (tablebase::Contains(next_state, layer)) {
                state = next_state;
                tb_rotations[tb_rotations.size()-layer-1] = Rotations(rotation);
                break;
            }
        }
    }
}


// recursive solution
// bfs-like
void SolutionSearch(std::vector<Rotations>& search_rotations, int depth, State state) {
    std::map<State, uint8_t> visited;
    std::set<State> current_level;
    std::set<State> next_level;
    current_level.insert(state);

    int last_depth = depth;
    while (!current_level.empty() && depth != 0) {
        State cur_state = *current_level.begin();
        current_level.erase(current_level.begin());
        uint64_t corner_heuristic = corner::GetHeuristic(cur_state.corner_pos, cur_state.corner_orient);
        for (uint8_t rotation = 0; rotation < kNumRot; rotation++) {
            if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
                continue;
            }
            State next_state = cur_state;
            corner::Rotate(next_state.corner_pos, next_state.corner_orient, rotation);
            edge::Rotate(next_state.edge_pos, next_state.edge_sym, next_state.edge_orient, rotation);
            transposition_table::InTT in_tt = transposition_table::Contains(next_state, 2*(depth-1));
            if (in_tt == transposition_table::InTT::kFalse) {
                continue;
            }
            if (in_tt == transposition_table::InTT::kTrue) {
                search_rotations[depth-1] = Rotations(GetRevRotation(rotation));
                int temp_depth = depth;
                while (last_depth > temp_depth) {
                    uint8_t to_rotation = visited[cur_state];
                    corner::Rotate(cur_state.corner_pos, cur_state.corner_orient, GetRevRotation(to_rotation));
                    edge::Rotate(cur_state.edge_pos, cur_state.edge_sym, cur_state.edge_orient, GetRevRotation(to_rotation));
                    search_rotations[temp_depth] = Rotations(GetRevRotation(to_rotation));
                    temp_depth++;
                }
                last_depth = depth-1;
                visited.clear();
                current_level.clear();
                next_level.clear();
                next_level.insert(next_state);
                break;
            }
            // this is only really rarely the case and thus most of the time this function should be really fast
            if (in_tt == transposition_table::InTT::kCollision || in_tt == transposition_table::InTT::kHighDepth) {
                next_level.insert(next_state);
                visited.insert(std::make_pair(next_state, rotation));
                continue;
            }
        }
        if (current_level.empty()) {
            depth--;
            std::swap(current_level, next_level);
        }
    }
}


// check if the state is already in tablebase
// return layer
// else return -1
int GetTBLayer(const State& state) {
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (tablebase::Contains(state, i)) {
            LOG_EXTRA("Position in tablebase");
            return i;
        }
    }
    return -1;
}



void DFSNextFrontierSearch (const State& state, Frontier& next_frontier,
                            SharedLeafSolution& shared_leaf_solution,
                            SharedLeafStates& shared_leaf_states,
                            LocalBuffer& local_buffer,
                            uint64_t& num_positions_search, uint8_t cur_depth, uint8_t depth, int thread_idx) {
    if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
        return;
    }

    bool frontier_insert = false;
    // rotate to the next position
    uint64_t corner_heuristic = corner::GetHeuristic(state.corner_pos, state.corner_orient);
    for (uint8_t rotation = 0; rotation < kNumRot; rotation++) {
        if (((corner_heuristic >> (8+2*rotation)) & 3) == 3) { // illegal rotation
            continue;
        }

        State next_state = state;
        corner::Rotate(next_state.corner_pos, next_state.corner_orient, rotation);
        edge::Rotate(next_state.edge_pos, next_state.edge_sym, next_state.edge_orient, rotation);
        num_positions_search++;

        // already visited
        // if not insert this position
        if (transposition_table::Contains(next_state, 2*(cur_depth+1)) == transposition_table::InTT::kTrue) {
            continue;
        }

        uint8_t max_heuristic = std::max(edge::GetHeuristic(next_state.edge_pos, next_state.edge_orient),
                                         uint8_t(uint8_t(corner_heuristic) + ((corner_heuristic >> (8+2*rotation)) & 3) - 1));

        // in tablebase
        if (max_heuristic <= Settings::GetTBDepth() && tablebase::Contains(next_state)) {
            bool expected = false;
            if (shared_leaf_solution.finished.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
                shared_leaf_states.cv.notify_all();
                shared_leaf_solution.state = next_state;
            }
            LOG_EXTRA("found solution of length ", Settings::GetTBDepth()+cur_depth+1);
            return;
        }

        // due to the heuristic it is not possible to solve the next state in fewer moves than the depth
        // this means that the state has to be again part of the new frontier
        if (std::max(max_heuristic, uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth) {
            frontier_insert = true;
            continue;
        }

        // send the position to GPU search
        // this means that the state has to be again part of the new frontier
        if (std::max(max_heuristic, uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth - 3 &&
            depth - cur_depth - 1 - Settings::GetTBDepth() < 16 &&  // fits in the rotation registers
            cur_depth + 1 > 4 &&  // more than 5 moves need to be already made
            depth - cur_depth - 1 - Settings::GetTBDepth() < 10) { // this value can be tweeked to have more cpu calculation needed
            if (transposition_table::InsertLeaf(next_state, depth - cur_depth)) {
                continue;
            }

            local_buffer->push_back({next_state, cur_depth + 1});

            // insert local buffer when there is space
            if (local_buffer->size() >= size_t(Settings::GetNumPositionsPerBatch())) {
                std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
                shared_leaf_states.cv.wait(lock, [&] {
                    return int(shared_leaf_states.shared_ptrs.size()) <= Settings::GetNumParallelBatches() || shared_leaf_solution.finished.load(std::memory_order_acquire);
                });
                shared_leaf_states.shared_ptrs.push(std::move(local_buffer));
                local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();
            }

            frontier_insert = true;
            continue;
        }

        // insert into visited_search
        transposition_table::Insert(next_state, 2*(cur_depth+1));


        // Do further DFS
        DFSNextFrontierSearch(next_state, next_frontier, shared_leaf_solution, shared_leaf_states, local_buffer, num_positions_search, cur_depth+1, depth, thread_idx);
    }

    if (frontier_insert) {
        uint8_t heuristic = uint8_t(corner_heuristic) + edge::GetHeuristic(state.edge_pos, state.edge_orient);
        next_frontier[heuristic][thread_idx].push_back({state, cur_depth});
    }
}


void FrontierSearch (uint64_t& num_positions_search, SharedLeafSolution& shared_leaf_solution,
                     SharedLeafStates& shared_leaf_states, uint8_t depth,
                     Frontier& cur_frontier, Frontier& next_frontier, std::atomic<long long>& frontier_idx, int thread_idx) {
    // local buffer for leaf search
    LocalBuffer local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();

    long long accumulator = 0;
    int heuristic_layer = 0;
    int thread_layer = 0;

    while (size_t(heuristic_layer) < cur_frontier.size()) {
        long long cur_idx = frontier_idx++;
        while (size_t(cur_idx - accumulator) >= cur_frontier[heuristic_layer][thread_layer].size()) {
            accumulator += cur_frontier[heuristic_layer][thread_layer].size();
            thread_layer++;
            if (thread_layer == Settings::GetNumThreads()) {
                thread_layer = 0;
                heuristic_layer++;
            }
            if (size_t(heuristic_layer) >= cur_frontier.size()) {
                break;
            }
        }
        if (size_t(heuristic_layer) >= cur_frontier.size()) {
            break;
        }
        if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
            break;
        }

        DFSNextFrontierSearch(cur_frontier[heuristic_layer][thread_layer][cur_idx-accumulator].first, next_frontier, shared_leaf_solution, shared_leaf_states, local_buffer, num_positions_search, cur_frontier[heuristic_layer][thread_layer][cur_idx-accumulator].second, depth, thread_idx);
    }

    // insert element if the search is not finished with the current level
    if (local_buffer->size() > 0) {
        std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
        shared_leaf_states.cv.wait(lock, [&] {
            return int(shared_leaf_states.shared_ptrs.size()) <= Settings::GetNumParallelBatches() || shared_leaf_solution.finished.load(std::memory_order_acquire);
        });
        shared_leaf_states.shared_ptrs.push(std::move(local_buffer));
        local_buffer = std::make_shared<std::vector<std::pair<State, uint8_t>>>();
    }
}


void BaseSearchManager (
    SharedSearch& shared_search,
    SharedLeafSolution& shared_leaf_solution,
    SharedLeafStates& shared_leaf_states,
    ThreadPool& search_thread_pool,
    State state, int scramble_idx,
    uint8_t& solution_depth,
    uint64_t& total_num_positions
) {
    solution_depth = std::max({uint8_t(corner::GetHeuristic(state.corner_pos, state.corner_orient)),
                                  edge::GetHeuristic(state.edge_pos, state.edge_orient),
                                  uint8_t(Settings::GetTBDepth()+1)}) + 1;

    uint64_t num_positions_search = 0;

    Frontier cur_frontier(kNumHeuristicLayers, std::vector<std::vector<std::pair<State, uint8_t>>>(Settings::GetNumThreads()));
    cur_frontier[0][0].push_back({state, 0});

    shared_leaf_states.scramble_idx = scramble_idx;
    for (; solution_depth < 254; solution_depth++) {
        shared_leaf_states.depth = solution_depth;
        shared_search.start_work->arrive_and_wait();
        LOG_EXTRA("Start with depth", solution_depth);

        // Search
        Frontier next_frontier(kNumHeuristicLayers, std::vector<std::vector<std::pair<State, uint8_t>>>(Settings::GetNumThreads()));
        std::vector<uint64_t> num_positions_search_threads(Settings::GetNumThreads(), 0);
        std::atomic<long long> frontier_idx = 0;
        search_thread_pool.Run([&](size_t thread_id){FrontierSearch(num_positions_search_threads[thread_id], shared_leaf_solution, shared_leaf_states, solution_depth, cur_frontier, next_frontier, frontier_idx, thread_id);});
        num_positions_search += std::accumulate(num_positions_search_threads.begin(), num_positions_search_threads.end(), 0ULL);
        std::swap(next_frontier, cur_frontier);

        // wait until queue is empty
        {
            std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
            shared_leaf_states.cv.wait(lock, [&] {
                return shared_leaf_states.shared_ptrs.empty() || shared_leaf_solution.finished.load(std::memory_order_acquire);
            });
        }
        LOG_EXTRA("start with finishing search");

        // finished search
        shared_leaf_states.finished_depth.store(true);
        shared_search.done_work->arrive_and_wait();
        shared_leaf_states.finished_depth.store(false);

        LOG_EXTRA("number positions:", num_positions_search + shared_search.leaf_cnt.load(), "search positions", num_positions_search, "leaf positions", shared_search.leaf_cnt.load());

        // found optimal solution
        if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
            LOG_EXTRA("Proven optimal solution");
            break;
        }
    }
    solution_depth--;

    uint64_t num_positions_leaf = shared_search.leaf_cnt.load();
    shared_search.leaf_cnt.store(0);
    shared_leaf_solution.finished.store(false);
    // delete all remaining elements of the queue
    std::queue<std::shared_ptr<std::vector<std::pair<State, uint8_t>>>>().swap(shared_leaf_states.shared_ptrs);

    total_num_positions = num_positions_leaf + num_positions_search;
}


void SearchManager () {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    // random starting positions
    LOG_EXTRA("Start calculating random positions");
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), Settings::GetRunOffset());
    LOG_ALL("Calculated random positions");

    // start timing
    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();

    uint64_t acc_total_num_positions = 0;
    double acc_depth = 0;

    // general barriers and stopping mechanisms
    SharedSearch shared_search;
    // state of the solution between the leaf search and the tb
    SharedLeafSolution shared_leaf_solution = {{false}, State()};
    // queue shared between base and leaf search
    SharedLeafStates shared_leaf_states;

    // start leaf search on CPU or GPU
    std::vector<std::jthread> leaf_search_threads;
    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        shared_search.start_work.emplace(Settings::GetDeviceCount()+1);
        shared_search.done_work.emplace(Settings::GetDeviceCount()+1);
        for (int i = 0; i < Settings::GetDeviceCount(); i++) {
            leaf_search_threads.emplace_back(DeviceLeafManagerInit, i, std::ref(shared_search), std::ref(shared_leaf_states), std::ref(shared_leaf_solution));
        }
    }
    #endif // USE_CUDA
    if (!Settings::UseCuda()) {
        shared_search.start_work.emplace(Settings::GetNumThreads()+1);
        shared_search.done_work.emplace(Settings::GetNumThreads()+1);
        LOG_CRITICAL("NOT YET SUPPORTED!");
    }

    // start base search on CPU
    ThreadPool search_thread_pool(Settings::GetNumThreads());
    for (size_t idx = 0; idx < random_positions.size(); idx++) {
        // already in TB
        if (GetTBLayer(random_positions[idx]) >= 0) {
            std::vector<Rotations> tb_rotations(GetTBLayer(random_positions[idx]));
            SolutionTB(tb_rotations, GetTBLayer(random_positions[idx]), random_positions[idx]);
            LOG_EXTRA("Proven optimal solution");
            LOG_EXTRA("Position already in tablebase");
            LOG_EXTRA("solution moves:", tb_rotations);
            LOG_ALL(SkipSpace("["), SkipSpace(idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", GetTBLayer(random_positions[idx]), "num_positions: 0");
            continue;
        }

        // Transposition Table
        search_thread_pool.Run([](size_t thread_id){transposition_table::Clear(thread_id, Settings::GetNumThreads());});
        transposition_table::Insert(random_positions[idx], 0);
        upload_to_leaf_map.clear();

        // Search
        uint8_t solution_depth = 0;
        uint64_t total_num_positions = 0;
        BaseSearchManager(shared_search, shared_leaf_solution, shared_leaf_states, search_thread_pool,
                          random_positions[idx], idx, solution_depth, total_num_positions);

        acc_depth += solution_depth;
        acc_total_num_positions += total_num_positions;

        // guarantie that the starting position is in TT
        transposition_table::Insert<true>(random_positions[idx], 0);

        // reconstruct the solution
        std::vector<Rotations> solution_rotations(solution_depth);
        SolutionTB(solution_rotations, Settings::GetTBDepth(), shared_leaf_solution.state);
        SolutionSearch(solution_rotations, solution_depth-Settings::GetTBDepth(), shared_leaf_solution.state);

        LOG_EXTRA("solution moves:", solution_rotations);

        LOG_ALL(SkipSpace("["), SkipSpace(idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", solution_depth, "num_positions:", total_num_positions);
        LOG_MEMORY();

        if (Logger::GetLoggerLevel() >= LoggerLevel::kExtra) { // test if the solution works
            State test_state = random_positions[idx];
            for (Rotations rotation : solution_rotations) {
                corner::Rotate(test_state.corner_pos, test_state.corner_orient, uint8_t(rotation));
                edge::Rotate(test_state.edge_pos, test_state.edge_sym, test_state.edge_orient, uint8_t(rotation));
            }
            if (test_state != kSolvedState) {
                LOG_WARNING("Not correct solution!");
            }
        }
    }
    shared_search.finished.store(true);
    shared_search.start_work->arrive_and_wait();

    // get the duration in milliseconds
    std::chrono::time_point end_time = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds elapsed_millis = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    LOG_ALL("Average time:", elapsed_millis.count()/Settings::GetNumRuns(), "ms");
    LOG_ALL("Average depth:", acc_depth/Settings::GetNumRuns());
    LOG_ALL("Average number of positions:", acc_total_num_positions/Settings::GetNumRuns());
    LOG_ALL("Positions per seconds:", acc_total_num_positions * 1000 / elapsed_millis.count());

}
