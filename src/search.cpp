#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numeric>
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
            transposition_table::InTT in_tt = transposition_table::Contains(next_state, depth-1);
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
        if (transposition_table::Contains(next_state, cur_depth+1) == transposition_table::InTT::kTrue) {
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
        /*
        if (std::max(max_heuristic, uint8_t(Settings::GetTBDepth()+1)) + cur_depth + 1 >= depth - 3 &&
            depth - cur_depth - 1 - Settings::GetTBDepth() < 16 &&  // fits in the rotation registers
            cur_depth + 1 > 5 &&  // more than 5 moves need to be already made
            depth - cur_depth - 1 - Settings::GetTBDepth() < 10) { // this value can be tweeked to have more cpu calculation needed

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
        */

        // insert into visited_search
        transposition_table::Insert(next_state, cur_depth+1);


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


void SearchManager () {
    if (Settings::GetNumRuns() <= 0) {
        return;
    }

    int num_leaf_threads = Settings::GetNumThreads();
    #ifdef USE_CUDA
    if (Settings::UseCuda()) {
        num_leaf_threads = Settings::GetNumGPUUploadThreads();
    }
    #endif
    // ThreadPool leaf_thread_pool(num_leaf_threads);
    ThreadPool search_thread_pool(Settings::GetNumThreads());

    // start timing
    std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();

    // random starting positions
    LOG_EXTRA("Start calculating random positions");
    std::vector<State> random_positions = RandomPositions(Settings::GetNumRuns(), Settings::GetRunOffset());
    LOG_ALL("Calculated random positions");

    // accumulated positions for information purposes
    uint64_t acc_total_num_positions = 0;
    double acc_depth = 0;

    for (size_t random_positions_idx = 0; random_positions_idx < random_positions.size(); random_positions_idx++) {
        // already in TB
        if (GetTBLayer(random_positions[random_positions_idx]) >= 0) {
            std::vector<Rotations> tb_rotations;
            SolutionTB(tb_rotations, GetTBLayer(random_positions[random_positions_idx]), random_positions[random_positions_idx]);
            LOG_EXTRA("Proven optimal solution");
            LOG_EXTRA("Position already in tablebase");
            LOG_EXTRA("solution moves:", tb_rotations);
            LOG_ALL(SkipSpace("["), SkipSpace(random_positions_idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", GetTBLayer(random_positions[random_positions_idx]), "num_positions: 0");
            continue;
        }
        // Transposition Table
        search_thread_pool.Run([](size_t thread_id){transposition_table::Clear(thread_id, Settings::GetNumThreads());});
        transposition_table::Insert(random_positions[random_positions_idx], 0);

        // queue shared between search
        SharedLeafStates shared_leaf_states;

        // state of the solution between the leaf search and the tb
        SharedLeafSolution shared_leaf_solution = {{false}, State()};

        Frontier cur_frontier(kNumHeuristicLayers, std::vector<std::vector<std::pair<State, uint8_t>>>(Settings::GetNumThreads()));
        cur_frontier[0][0].push_back({random_positions[random_positions_idx], 0});

        // information purposes
        uint64_t total_num_positions = 0;
        uint64_t num_positions_search = 0;
        uint64_t num_positions_leaf = 0;


        // go over the different depths (iterative deepening)
        State state = random_positions[random_positions_idx];
        uint8_t id_depth = std::max(uint8_t(corner::GetHeuristic(state.corner_pos, state.corner_orient)), edge::GetHeuristic(state.edge_pos, state.edge_orient))+1;

        for (; true; id_depth++) {
            LOG_EXTRA("Start with depth", id_depth);

            // Start LeafManagers on seperate threads
            std::vector<uint64_t> num_positions_leaf_threads(num_leaf_threads, 0);

            std::atomic<bool> leaf_stoken{false};
            /*
            #ifdef USE_CUDA
            if (Settings::UseCuda()) {
                CudaConstMemChangeCurDepth(id_depth);

                leaf_thread_pool.AsyncRun([&](size_t thread_id){DeviceLeafManager(leaf_stoken, shared_leaf_states, shared_leaf_solution, num_positions_leaf_threads[thread_id], thread_id, id_depth, random_positions_idx);});
            }
            #endif
            */
            // is always off if it is compiled without cuda
            if (!Settings::UseCuda()) {
                LOG_CRITICAL("currently not supported");
            }

            // Search
            Frontier next_frontier(kNumHeuristicLayers, std::vector<std::vector<std::pair<State, uint8_t>>>(Settings::GetNumThreads()));
            std::vector<uint64_t> num_positions_search_threads(Settings::GetNumThreads(), 0);
            std::atomic<long long> frontier_idx = 0;
            search_thread_pool.Run([&](size_t thread_id){FrontierSearch(num_positions_search_threads[thread_id], shared_leaf_solution, shared_leaf_states, id_depth, cur_frontier, next_frontier, frontier_idx, thread_id);});
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

            // Stop LeafManager
            leaf_stoken = true;
            // leaf_thread_pool.Wait();
            for (int i = 0; i < num_leaf_threads; i++) {
                num_positions_leaf += num_positions_leaf_threads[i];
            }

            total_num_positions = num_positions_search + num_positions_leaf;
            LOG_EXTRA("number positions:", total_num_positions, "search positions", num_positions_search, "leaf positions", num_positions_leaf);

            // found optimal solution
            if (shared_leaf_solution.finished.load(std::memory_order_acquire)) {
                LOG_EXTRA("Proven optimal solution");
                break;
            }

        }

        id_depth--;
        acc_depth += id_depth;
        acc_total_num_positions += total_num_positions;

        // guarantie that the starting position is in TT
        transposition_table::Insert<true>(random_positions[random_positions_idx], 0);

        // Tablebase
        std::vector<Rotations> solution_rotations(id_depth);
        SolutionTB(solution_rotations, Settings::GetTBDepth(), shared_leaf_solution.state);
        SolutionSearch(solution_rotations, id_depth-Settings::GetTBDepth(), shared_leaf_solution.state);

        LOG_EXTRA("solution moves:", solution_rotations);

        LOG_ALL(SkipSpace("["), SkipSpace(random_positions_idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", int(id_depth), "num_positions:", total_num_positions);
        LOG_MEMORY();

        if (Logger::GetLoggerLevel() >= LoggerLevel::kExtra) { // test if the solution works
            State test_state = random_positions[random_positions_idx];
            for (Rotations rotation : solution_rotations) {
                corner::Rotate(test_state.corner_pos, test_state.corner_orient, uint8_t(rotation));
                edge::Rotate(test_state.edge_pos, test_state.edge_sym, test_state.edge_orient, uint8_t(rotation));
            }
            if (test_state != kSolvedState) {
                LOG_WARNING("Not correct solution!");
            }
        }
    }

    // get the duration in milliseconds
    std::chrono::time_point since_epoch = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch - start_time);

    // Some informations
    LOG_ALL("Average time:", millis.count()/Settings::GetNumRuns(), "ms");
    LOG_ALL("Average depth:", acc_depth/Settings::GetNumRuns());
    LOG_ALL("Average number of positions:", acc_total_num_positions/Settings::GetNumRuns());
    LOG_ALL("Positions per seconds:", acc_total_num_positions * 1000 / millis.count());
}
