#include <cstdint>


#include "actions.h"
#include "cube.h"
#include "error_handler.h"


bool Search(ErrorHandler error_handler, Actions& actions, Cube& cube, int depth, uint64_t& num_positions);
