#include <cstdint>
#include <unordered_map>
#include <vector>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "rotation.h"
#include "search.h"


// iterative deepening depth first search with heuristic function
bool Search (ErrorHandler error_handler, Actions& actions, Cube& cube, int depth,
        uint64_t& num_positions, std::unordered_map<std::pair<unsigned int, uint64_t>, int, PositionHash>& visited) {
    // check if this position has been already visited
    auto saved_depth = visited.find({cube.GetPositionHash(), cube.GetEdgeHash()});
    if (saved_depth != visited.end() && saved_depth->second >= depth) {
        return false;
    }

    num_positions++;

    // stop if the solved position cannot be reached in the time
    if (depth - cube.GetHeuristicFunction() < 0 || actions.stop) {
        return false;
    }

    // solved positions
    if (cube.IsSolved()) {
        return true;
    }

    visited[{cube.GetPositionHash(), cube.GetEdgeHash()}] = depth;

    // dfs
    std::vector<Rotations> legal_rotations = GetLegalRotations(cube);
    for (Rotations rotation : legal_rotations) {
        Cube next_cube = Rotate(cube, rotation);
        if (Search(error_handler, actions, next_cube, depth-1, num_positions, visited)) {
            actions.solve.push(rotation);
            return true;
        }
    }

    return false;
}
