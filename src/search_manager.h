#pragma once

#include "actions.h"
#include "error_handler.h"
#include "settings.h"


void SearchManager (ErrorHandler error_handler, Setting& settings, Actions& actions, std::mt19937& rng);
