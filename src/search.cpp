#include <cstdint>
#include <vector>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"


// iterative deepening depth first search with heuristic function
bool Search (ErrorHandler error_handler, Actions& actions, Cube& cube, int depth, uint64_t& num_positions) {
    num_positions++;

    // stop if the solved position cannot be reached in the time
    if (depth - cube.GetHeuristicFunction() < 0 || actions.stop) {
        return false;
    }

    // solved positions
    if (cube.IsSolved()) {
        return true;
    }

    // dfs
    std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
    for (Rotations rotation : legal_rotations) {
        Cube next_cube = Rotate(cube, rotation);
        if (Search(error_handler, actions, next_cube, depth-1, num_positions)) {
            actions.solve.push(rotation);
            return true;
        }
    }

    return false;
}
