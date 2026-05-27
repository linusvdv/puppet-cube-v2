#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>


constexpr int kNumRotations = 18;


void RotationInit();


struct RotationRepresentations {
    std::string name;
    uint8_t index; // internal representation

    std::array<std::array<int, 3>, 3> matrix;
    int activate; // which pieces are affected
};


extern std::map<std::string, RotationRepresentations> name_to_rotation_representations;
extern std::map<uint8_t, RotationRepresentations> index_to_rotation_representations;
extern std::map<std::pair<std::array<std::array<int, 3>, 3>, int>, RotationRepresentations> matrix_to_rotation_representations;
