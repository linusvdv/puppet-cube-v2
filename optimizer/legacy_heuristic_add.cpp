#include <iostream>
#include <vector>
#include <cmath>
#include <cstdio>
#include "cuddObj.hh" // CUDD C++ Wrapper

// Divide and Conquer function to build the ADD
ADD BuildADDFromVector(Cudd& mgr, const std::vector<uint64_t>& data, 
                       const std::vector<ADD>& vars,
                       size_t start, size_t end, int var_idx, size_t actual_size) {
    
    // 1. Handle Out-of-Bounds Padding
    // If the entire block is beyond our actual data, return a "Don't Care" value.
    // Setting this to a dummy value (like 255) lets CUDD merge all invalid states into one node.
    if (start >= actual_size) {
        return mgr.constant(255); 
    }

    // 2. Base Case: Leaf Node (Single element)
    if (start + 1 == end) {
        return mgr.constant(data[start]);
    }

    // 3. Optimization: Uniformity Check
    // If all elements in this chunk are identical, return a single constant node.
    // This dramatically reduces intermediate RAM usage during construction.
    bool uniform = true;
    uint64_t first_val = data[start];
    size_t check_end = std::min(end, actual_size);
    for (size_t i = start + 1; i < check_end; ++i) {
        if (data[i] != first_val) {
            uniform = false;
            break;
        }
    }
    if (uniform && check_end == end) {
        return mgr.constant(first_val);
    }

    // 4. Recursive Split
    size_t mid = start + (end - start) / 2;
    ADD low = BuildADDFromVector(mgr, data, vars, start, mid, var_idx + 1, actual_size);
    ADD high = BuildADDFromVector(mgr, data, vars, mid, end, var_idx + 1, actual_size);

    // 5. Node Creation
    // If both halves map to the exact same ADD, bypass creating a decision node.
    if (low == high) {
        return low;
    }

    // condition.Ite(then_branch, else_branch)
    // If the variable is 1, go to 'high' (upper half of array), else go to 'low'.
    return vars[var_idx].Ite(high, low); 
}

int main() {
    const std::string path = "../precomputation/edge_heuristic.bin";
    
    // Reusing your constants
    constexpr int kNumEdgeOrientation = 2048;
    constexpr int kNumEdgePositions = 9985968;
    constexpr size_t kNumEdgeHeuristic = (size_t)kNumEdgePositions * kNumEdgeOrientation / 16; // ~1.36 Billion

    std::vector<uint64_t> data(kNumEdgeHeuristic);

    // Simplified file reading
    std::cout << "Loading file..." << std::endl;
    if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
        if (std::fread(data.data(), sizeof(uint64_t), kNumEdgeHeuristic, file) != kNumEdgeHeuristic) {
            std::cerr << "Failed to read full file!" << std::endl;
            return 1;
        }
        std::fclose(file);
    } else {
        std::cerr << "Could not open file: " << path << std::endl;
        return 1;
    }

    std::cout << "File loaded. Initializing CUDD..." << std::endl;

    // Initialize CUDD Manager
    // Arguments: numVars, numVarsZ, numSlots, cacheSize, maxMemory
    // 0 defaults let CUDD manage its own sizes dynamically.
    Cudd mgr(0, 0); 

    // Calculate how many binary variables are needed to represent the indices
    int num_vars = std::ceil(std::log2(kNumEdgeHeuristic));
    size_t padded_size = 1ULL << num_vars; // Pad to next power of 2 (2^31)

    std::vector<ADD> vars;
    for (int i = 0; i < num_vars; ++i) {
        vars.push_back(mgr.addVar(i)); // Create binary variables for the ADD
    }

    std::cout << "Building ADD from array... (This may take a while)" << std::endl;
    
    // Build the tree
    ADD pdb_add = BuildADDFromVector(mgr, data, vars, 0, padded_size, 0, kNumEdgeHeuristic);

    // Output Memory Statistics
    std::cout << "========================================" << std::endl;
    std::cout << "Final ADD Node Count: " << pdb_add.nodeCount() << " nodes" << std::endl;
    
    // Note: CUDD memory includes overhead from the manager's hash tables.
    double mem_mb = mgr.ReadMemoryInUse() / (1024.0 * 1024.0);
    std::cout << "Total CUDD Manager Memory: " << mem_mb << " MB" << std::endl;

    return 0;
}
