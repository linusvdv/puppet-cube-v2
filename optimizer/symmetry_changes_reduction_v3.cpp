#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

const uint16_t WILDCARD = 65535;
const int VECTOR_SIZE = 48;

struct VectorNode {
    int original_index;
    std::vector<uint16_t> data;
    std::vector<int> conflicts; // Adjacency list: indices of vectors this cannot pair with
    int degree = 0;
};

// Returns true if two vectors conflict (have different explicit numbers at the same index)
bool has_conflict(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
    for (int i = 0; i < VECTOR_SIZE; ++i) {
        if (a[i] != WILDCARD && b[i] != WILDCARD && a[i] != b[i]) {
            return true;
        }
    }
    return false;
}

// Merges a vector into a growing group representative
void merge_into_representative(std::vector<uint16_t>& rep, const std::vector<uint16_t>& vec) {
    for (int i = 0; i < VECTOR_SIZE; ++i) {
        if (rep[i] == WILDCARD) {
            rep[i] = vec[i];
        }
    }
}

int main() {
    // Populate with your 2861 vectors
    std::vector<std::vector<uint16_t>> raw_input;
    for (int i = 0; i < 2861; i++) {
        std::vector<uint16_t> nums(48);
        for (uint16_t& num : nums) {
            std::cin >> num;
        }
        raw_input.push_back(nums);
    }


    int n = raw_input.size();
    std::vector<VectorNode> nodes(n);
    
    std::cout << "Step 1: Building global conflict matrix..." << std::endl;
    for (int i = 0; i < n; ++i) {
        nodes[i].original_index = i;
        nodes[i].data = raw_input[i];
    }

    // Build the structural conflict graph
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (has_conflict(nodes[i].data, nodes[j].data)) {
                nodes[i].conflicts.push_back(j);
                nodes[j].conflicts.push_back(i);
                nodes[i].degree++;
                nodes[j].degree++;
            }
        }
    }

    std::cout << "Step 2: Sorting nodes by conflict density (Welsh-Powell)..." << std::endl;
    // Sort descending by degree: highly restrictive vectors handled first
    std::sort(nodes.begin(), nodes.end(), [](const VectorNode& a, const VectorNode& b) {
        return a.degree > b.degree;
    });

    std::cout << "Step 3: Executing Global Aggregation..." << std::endl;
    std::vector<int> assignments(n, -1);
    std::vector<std::vector<uint16_t>> group_representatives;
    std::vector<std::vector<int>> group_members;
    
    int current_group_id = 0;

    for (int i = 0; i < n; ++i) {
        if (assignments[nodes[i].original_index] != -1) continue; // Already grouped

        // Start a new group
        int group_id = current_group_id++;
        std::vector<uint16_t> current_rep = nodes[i].data;
        
        assignments[nodes[i].original_index] = group_id;
        std::vector<int> members = { nodes[i].original_index };

        // Scan the entire rest of the dataset to pull everything possible into this group
        for (int j = i + 1; j < n; ++j) {
            int target_idx = nodes[j].original_index;
            if (assignments[target_idx] != -1) continue; // Skip already paired items

            // Check if this vector conflicts with the current actively evolving representative
            if (!has_conflict(current_rep, nodes[j].data)) {
                merge_into_representative(current_rep, nodes[j].data);
                assignments[target_idx] = group_id;
                members.push_back(target_idx);
            }
        }
        
        group_representatives.push_back(current_rep);
        group_members.push_back(members);
    }

    // Post-processing: Replace any leftover wildcards with 0
    for (auto& rep : group_representatives) {
        for (auto& val : rep) {
            if (val == WILDCARD) val = 0;
        }
    }

    std::cout << "\n>>> CONSOLIDATION COMPLETE <<<" << std::endl;
    std::cout << "Total Groups Generated: " << group_representatives.size() << std::endl;

    // Print out the largest groups found
    std::cout << "\nTop 5 Largest Groups:" << std::endl;
    for (size_t i = 0; i < std::min((size_t)5, group_representatives.size()); ++i) {
        std::cout << "Group #" << i << " (Contains " << group_members[i].size() << " vectors) -> ";
        for (int k = 0; k < 10; ++k) std::cout << group_representatives[i][k] << " "; // first 10 elements
        std::cout << "...\n";
    }

    return 0;
}
