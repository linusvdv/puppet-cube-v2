#pragma once
#include "cube.hpp"


using TablebasePrecomputation = phmap::parallel_flat_hash_set<Cube::State,
    phmap::priv::hash_default_hash<Cube::State>,
    phmap::priv::hash_default_eq<Cube::State>,
    phmap::priv::Allocator<Cube::State>,
    12, std::mutex>;


struct Tablebase {
    static void Initialize();


private:
    static std::vector<std::vector<Cube::State>> tablebase;
};
