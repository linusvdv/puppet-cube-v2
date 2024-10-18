#pragma once

#include "actions.h"
#include "cube.h"
#include "error_handler.h"


int TablebaseDepth (Cube& cube);


bool TablebaseContainsOuter (Cube::Hash hash);


bool TablebaseSolve (Cube& cube, Actions& actions, int depth, uint64_t& num_positions);


void TablebaseSearch (ErrorHandler error_handler, int depth);


int GetTablebaseDepth ();
