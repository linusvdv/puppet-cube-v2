#include <vector>
#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"


// iterative deepening depth first search with heuristic function
bool Search (ErrorHandler error_handler, Actions& actions, Cube& cube, int depth) {
    // stop if the solved position cannot be reached in the time
    if (depth - cube.GetHeuristicFunction() < 0 || actions.stop) {
        return false;
    }

    // solved positions
    if (cube.IsSolved()) {
        return true;
    }

    // dfs
    std::vector<Rotations> legal_rotations = GetLegalRotations(error_handler, cube);
    for (Rotations rotation : legal_rotations) {
        Cube next_cube = Rotate(cube, rotation);
        if (Search(error_handler, actions, next_cube, depth-1)) {
            actions.sove.push(rotation);
            return true;
        }
    }

    return false;
}
