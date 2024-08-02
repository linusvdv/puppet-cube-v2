#include <mutex>

#include "actions.h"


// returns true if queue is not empty
bool Actions::TryPop (Action& action) {
    // lock the mutex to use the queue
    std::lock_guard<std::mutex> guard(actions_mutex_);

    // only change the action when there is a new one
    if (actions_.empty()) {
        return false;
    }
    action = actions_.front();
    actions_.pop();
    return true;
}


// pushes next action
void Actions::Push (const Action& action) {
    // lock the mutex to use the queue
    std::lock_guard<std::mutex> guard(actions_mutex_);

    // add action to the queue of actions
    actions_.push(action);
}
