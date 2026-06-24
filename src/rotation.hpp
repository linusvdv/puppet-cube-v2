#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>


constexpr uint8_t kNumRot = 18;


void RotationInit();


struct RotRep {
    std::string name;
    uint8_t index; // internal representation

    std::array<std::array<int, 3>, 3> matrix;
    int activate; // which pieces are affected
};


extern std::map<std::string, RotRep> name_to_rot_rep;
extern std::array<RotRep, kNumRot> idx_to_rot_rep;
extern std::map<std::pair<std::array<std::array<int, 3>, 3>, int>, RotRep> mat_to_rot_rep;


inline uint8_t GetRevRotation(uint8_t rotation) {
    if (rotation % 2 == 0) {
        return rotation + 1;
    }
    return rotation - 1;
}
