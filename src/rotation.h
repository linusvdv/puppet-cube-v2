#pragma once
#include <atomic>
#include <mutex>
#include <queue>


// different rotations using standart notation
// c means counterclockwise
const int kNumRotations = 18;
enum Rotations {
    kR, kRc,
    kL, kLc,

    kU, kUc,
    kD, kDc,

    kF, kFc,
    kB, kBc,
// slice moves
    kM, kMc,
    kE, kEc,
    kS, kSc
};


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

private:
    // queue of next actions that the window manager
    std::queue<Action> actions_;
    // using mutexes to use multible threads
    std::mutex actions_mutex_;
};
