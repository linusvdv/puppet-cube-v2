#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

#include "BCHTSet.hpp"
#include "cube.hpp"
#include "logger.hpp"
#include "transposition_table.hpp"
#include "utils.hpp"
#include "random_position.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "tablebase.hpp"


#ifdef USE_CUDA
#include "search_bridge.hpp"
#endif  // USE_CUDA


void SolutionTB(std::vector<Rotations>& tb_rotations, int tb_layer, State state) {
    for (int layer = tb_layer - 1; layer >= 0; layer--) {
        for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
            State next_state = Cube::Rotate(state, rotation).second;
            if (BCHTSetContains(Tablebase::tablebase[layer], next_state)) {
                state = next_state;
                tb_rotations[tb_rotations.size()-tb_layer-1] = Rotations(rotation);
                break;
            }
        }
    }
}


// recursive solution
// bfs-like
bool SolutionSearch(std::vector<Rotations>& search_rotations, int depth, State state) {
    if (depth == 0) {
        // it is guarantied that the starting position is in the transposition table
        // so in_tt of the starting position is kTrue
        return false;
    }
    for (uint8_t rotation = 0; rotation < kNumRotations; rotation++) {
        State next_state = Cube::Rotate(state, rotation).second;
        InTT in_tt = TranspositionTable::ContainsState(next_state, depth-1);
        if (in_tt == InTT::kFalse) {
            continue;
        }
        if (in_tt == InTT::kTrue) {
            search_rotations[depth-1] = Rotations(GetRevRotation(rotation));
            SolutionSearch(search_rotations, depth-1, next_state);
            return true;
        }
        // this is only really rarly the case and thus most of the time this function should be really fast
        if (in_tt == InTT::kCollision) {
            if (SolutionSearch(search_rotations, depth-1, next_state)) {
                search_rotations[depth-1] = Rotations(GetRevRotation(rotation));
                return true;
            }
        }
    }
    return false;
}


// check if the state is already in tablebase
// return layer
// else return -1
int GetTBLayer(const State& state) {
    for (int i = 0; i <= Settings::GetTBDepth(); i++) {
        if (BCHTSetContains(Tablebase::tablebase[i], state)) {
            LOG_EXTRA("Position in tablebase");
            return i;
        }
    }
    return -1;
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
        TranspositionTable::Clear();
        TranspositionTable::InsertState(random_positions[random_positions_idx], 0);

        // queue shared between search
        SharedLeafStates shared_leaf_states;

        // state of the solution between the leaf search and the tb
        SharedLeafSolution shared_leaf_solution = {{false}, State()};

        // information purposes
        uint64_t total_num_positions = 0;
        uint64_t num_positions_search = 0;
        uint64_t num_positions_leaf = 0;


        // go over the different depths (iterative deepening)
        uint8_t id_depth = Cube().GetMaxHeuristic(random_positions[random_positions_idx])+1;

        for (; true; id_depth++) {
            LOG_EXTRA("Start with depth", id_depth);

            // Start LeafManagers on seperate threads
            std::vector<std::jthread> leaf_manager_threads;
            std::vector<uint64_t> num_positions_leaf_threads(num_leaf_threads, 0);

            #ifdef USE_CUDA
            if (Settings::UseCuda()) {
                CudaConstMemChangeCurDepth(id_depth);

                for (int i = 0; i < Settings::GetNumGPUUploadThreads(); i++) {
                    leaf_manager_threads.push_back(std::jthread(DeviceLeafManager, std::ref(shared_leaf_states), std::ref(shared_leaf_solution),
                                                                std::ref(num_positions_leaf_threads[i]), i, id_depth));
                }
            }
            #endif
            // is always off if it is compiled without cuda
            if (!Settings::UseCuda()) {
                LOG_ERROR("currently not supported");
                // LOG_CRITICAL("currently not supported");
            }

            // Search

            // wait until queue is empty
            {
                std::unique_lock<std::mutex> lock(shared_leaf_states.mtx);
                shared_leaf_states.cv.wait(lock, [&] {
                    return shared_leaf_states.shared_ptrs.empty() || shared_leaf_solution.finished.load(std::memory_order_acquire);
                });
            }
            LOG_EXTRA("start with finishing search");

            // Stop LeafManager
            for (int i = 0; i < num_leaf_threads; i++) {
                leaf_manager_threads[i].request_stop();
            }
            for (int i = 0; i < num_leaf_threads; i++) {
                leaf_manager_threads[i].join();
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

        // Tablebase
        std::vector<Rotations> solution_rotations(id_depth);
        SolutionTB(solution_rotations, Settings::GetTBDepth(), shared_leaf_solution.state);
        SolutionSearch(solution_rotations, id_depth-Settings::GetTBDepth(), shared_leaf_solution.state);

        LOG_EXTRA("solution moves:", solution_rotations);

        LOG_ALL(SkipSpace("["), SkipSpace(random_positions_idx+1), SkipSpace("/"), SkipSpace(Settings::GetNumRuns()), "] Depth:", int(id_depth), "num_positions:", total_num_positions);
        LOG_MEMORY();
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
