#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <stack>

#include "rotation.h"


// is in file rotation.h
enum Rotations : uint8_t;


enum Instructions {
    kRotation,
    kIsScrambling, // faster rotation speed
    kIsSolving,    // slower rotation speed
    kReset         // solved cube state
};


// actions for the rendering thread
struct Action {
    Instructions instruction;
    Rotations rotation;
};


class Actions {
public:
    // returns true if queue is not empty
    bool TryPop (Action& action);

    // pushes next action
    void Push (const Action& action);

    // stop program
    std::atomic<bool> stop = false;

    // solving the cube (back to front)
    std::stack<Rotations> solve;

private:
    // queue of next actions that the window manager
    std::queue<Action> actions_;
    // using mutexes to use multible threads
    std::mutex actions_mutex_;
};
