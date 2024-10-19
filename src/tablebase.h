#pragma once

#include "actions.h"
#include "cube.h"
#include "error_handler.h"
#include "settings.h"


int TablebaseDepth (Cube& cube);


bool TablebaseContainsOuter (Cube::Hash hash);


bool TablebaseSolve (Cube& cube, Actions& actions, int depth, uint64_t& num_positions);


void TablebaseSearch (ErrorHandler error_handler, Setting& settings, int depth);


int GetTablebaseDepth ();
