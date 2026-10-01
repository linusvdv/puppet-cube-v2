#include <vector>

#include "rotation.hpp"
#include "duplicate_rotations.hpp"


std::array<uint64_t, kDuplicateRotationDataSize> DuplicateRotations::data = {0, 0, 0, 0, 0, 0};

void DuplicateRotations::Initialize() {
    std::vector<std::pair<std::string, std::string>> unnecessary_rotatations = {
        // R, M, L
        {"R", "R'"},
        {"R'", "R"},
        {"M", "M'"},
        {"M'", "M"},
        {"L", "L'"},
        {"L'", "L"},
            // same as slice moves
        {"L'", "R"}, {"R", "L'"},
        {"L", "R'"}, {"R'", "L"},
        {"M'", "R"}, {"R", "M'"},
        {"M", "R'"}, {"R'", "M"},
            // two opposite rotations which end up in the same cube
        {"M", "M"},    // "M'", "M'"
        {"M", "R"},    // "R", "M"
        {"M'", "R'"}, // "R'", "M'"
        {"M", "L"},   // "L", "M"
        {"M'", "L'"}, // "L'", "M'"
        {"L", "R"},   // "R", "L"
        {"L'", "R'"}, // "R'", "L'"

        // U, E, D
        {"U", "U'"},
        {"U'", "U"},
        {"E", "E'"},
        {"E'", "E"},
        {"D", "D'"},
        {"D'", "D"},
            // same as slice moves
        {"D'", "U"}, {"U", "D'"},
        {"D", "U'"}, {"U'", "D"},
        {"E'", "U"}, {"U", "E'"},
        {"E", "U'"}, {"U'", "E"},
            // two opposite rotations which end up in the same cube
        {"E", "E"},    // "E'", "E'"
        {"E", "U"},    // "U", "E"
        {"E'", "U'"}, // "U'", "E'"
        {"E", "D"},   // "D", "E"
        {"E'", "D'"}, // "D'", "E'"
        {"D", "U"},   // "U", "D"
        {"D'", "U'"}, // "U'", "D'"

        // B, S, F
        {"B", "B'"},
        {"B'", "B"},
        {"S", "S'"},
        {"S'", "S"},
        {"F", "F'"},
        {"F'", "F"},
            // same as slice moves
        {"F'", "B"}, {"B", "F'"},
        {"F", "B'"}, {"B'", "F"},
        {"S'", "B"}, {"B", "S'"},
        {"S", "B'"}, {"B'", "S"},
            // two opposite rotations which end up in the same cube
        {"S", "S"},    // "S'", "S'"
        {"S", "B"},    // "B", "S"
        {"S'", "B'"}, // "B'", "S'"
        {"S", "F"},   // "F", "S"
        {"S'", "F'"}, // "F'", "S'"
        {"F", "B"},   // "B", "F"
        {"F'", "B'"}, // "B'", "F'"
    };
    for (auto& [first, second] : unnecessary_rotatations) {
        int index = (name_to_rot_rep[first].index * kNumRot) + name_to_rot_rep[second].index;
        data[index/64] |= uint64_t(1) << (index%64);
    }
}
