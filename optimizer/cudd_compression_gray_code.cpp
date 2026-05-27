#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cuddObj.hh>

using namespace std;

// The number of tracked edges. Set to 6 to match your request.
constexpr int NUM_EDGES = 7;
constexpr uint8_t UNREACHED = 255;
constexpr uint64_t STATE_SPACE_SIZE = 1ULL << (NUM_EDGES * 5); // 1 billion states
constexpr int NUM_MOVES = 18;

// Heap allocation to prevent stack overflow on 1GB array
vector<uint8_t>* dist_array;
vector<int> tables[NUM_MOVES];

// Helper to apply a move to the packed 30-bit integer
inline uint32_t apply_move(uint32_t state, int move) {
    uint32_t next_state = 0;
    for (int p = 0; p < NUM_EDGES; ++p) {
        int val = (state >> (p * 5)) & 31;
        next_state |= (tables[move][val] << (p * 5));
    }
    return next_state;
}

// Highly optimized recursive ADD builder. 
// It scans memory blocks to avoid creating unnecessary CUDD nodes.
ADD build_add_from_array(Cudd& mgr, int var, uint32_t prefix, int num_vars) {
    // Base case: Leaf node
    if (var == num_vars) {
        return mgr.constant((*dist_array)[prefix]);
    }
    
    // Optimization: Check if the entire memory block is identical
    uint32_t block_size = 1ULL << (num_vars - var);
    uint8_t first_val = (*dist_array)[prefix];
    bool uniform = true;
    for (uint32_t i = 1; i < block_size; ++i) {
        if ((*dist_array)[prefix + i] != first_val) {
            uniform = false;
            break;
        }
    }
    
    // If identical, return a constant node instantly.
    if (uniform) {
        return mgr.constant(first_val);
    }

    // Recursive step: split space into False/True branches for the current bit
    ADD low = build_add_from_array(mgr, var + 1, prefix, num_vars);
    ADD high = build_add_from_array(mgr, var + 1, prefix | (1ULL << (num_vars - 1 - var)), num_vars);

    ADD var_node = mgr.addVar(var);
    return var_node.Ite(high, low);
}

int main() {
    cout << "Allocating 1GB BFS Array..." << endl;
    dist_array = new vector<uint8_t>(STATE_SPACE_SIZE, UNREACHED);

    // 1. Load Tables
    ifstream in("tables.txt");
    if (!in.is_open()) {
        cerr << "Could not open tables.txt!" << endl;
        return 1;
    }

    vector<int> mapping(24);
    for (int i = 0; i < 24; ++i) in >> mapping[i];

    for (int m = 0; m < NUM_MOVES; ++m) {
        tables[m].resize(32);
        for (int i = 0; i < 32; ++i) in >> tables[m][i];
    }
    in.close();

    // 2. Compute Start State
    uint32_t start_state = 0;
    for (int p = 0; p < NUM_EDGES; ++p) {
        int solved_state = p * 2; // Slot 'p', Orientation 0
        uint32_t val = mapping[solved_state];
        start_state |= (val << (p * 5));
    }

    // 3. Explicit BFS Generation
    cout << "Starting BFS Generation..." << endl;
    vector<uint32_t> current_layer;
    current_layer.push_back(start_state);
    (*dist_array)[start_state] = 0;

    int depth = 0;
    long long accum = 0;

    while (!current_layer.empty()) {
        accum += current_layer.size();
        cout << "depth " << depth << " cnt " << accum << "\n";

        vector<uint32_t> next_layer;
        for (uint32_t s : current_layer) {
            for (int m = 0; m < NUM_MOVES; ++m) {
                uint32_t ns = apply_move(s, m);
                if ((*dist_array)[ns] == UNREACHED) {
                    (*dist_array)[ns] = depth + 1;
                    next_layer.push_back(ns);
                }
            }
        }
        current_layer = std::move(next_layer);
        depth++;
    }

    // 4. Initialize CUDD and Compress
    cout << "\nBFS Complete. Initializing CUDD..." << endl;
    Cudd mgr(0, 0);
    
    cout << "Building raw ADD from array..." << endl;
    int num_bits = NUM_EDGES * 5; // 30 bits
    ADD raw_add = build_add_from_array(mgr, 0, 0, num_bits);
    
    cout << "Raw ADD Nodes: " << raw_add.nodeCount() << endl;

    // 5. Pruning (Don't Cares)
    cout << "Pruning unreachable states..." << endl;

    // BddThreshold creates a BDD (1 for reachable, 0 for UNREACHED)
    BDD care_set_bdd = raw_add.BddThreshold(254.0);

    // Convert that BDD into an ADD so Restrict can accept it
    ADD care_set_add = care_set_bdd.Add();

    // Now Restrict will work!
    ADD pruned_add = raw_add.Restrict(care_set_add);

    cout << "Pruned ADD Nodes: " << pruned_add.nodeCount() << endl;
    cout << "\nSuccess! ADD is ready to be saved/used." << endl;

    delete dist_array;
    return 0;
}
