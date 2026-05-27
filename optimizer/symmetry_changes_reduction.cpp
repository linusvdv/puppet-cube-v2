#include <cstdint>
#include <iostream>
#include <vector>

const uint16_t WILDCARD = 65535;

// Structure to hold our dynamically growing groups
struct Group {
    std::vector<uint16_t> representative; // The evolving "merged" template
    std::vector<int> member_indices;      // Original indices of vectors in this group
};

// Checks if an input vector can fit into an existing group template, 
// and returns a merged version if true.
bool try_merge(const std::vector<uint16_t>& rep, const std::vector<uint16_t>& vec, std::vector<uint16_t>& merged) {
    merged.resize(rep.size());
    for (size_t i = 0; i < rep.size(); ++i) {
        if (rep[i] == WILDCARD) {
            merged[i] = vec[i]; // Take the value from the vector (could be a number or WILDCARD)
        } else if (vec[i] == WILDCARD) {
            merged[i] = rep[i]; // Keep the concrete value from the representative
        } else if (rep[i] != vec[i]) {
            return false; // Conflict found! They cannot be in the same group.
        } else {
            merged[i] = rep[i]; // They match
        }
    }
    return true;
}

int main() {
    // Your new test case where EVERY vector contains a wildcard '?'
    std::vector<std::vector<uint16_t>> input;
    for (int i = 0; i < 2861; i++) {
        std::vector<uint16_t> nums(48);
        for (uint16_t& num : nums) {
            std::cin >> num;
        }
        input.push_back(nums);
    }

    int num_vectors = input.size();
    std::vector<Group> groups;

    // Process every vector
    for (int i = 0; i < num_vectors; ++i) {
        bool placed = false;
        
        // Try to fit it into an existing group
        for (auto& group : groups) {
            std::vector<uint16_t> merged_rep;
            if (try_merge(group.representative, input[i], merged_rep)) {
                group.representative = merged_rep; // Sharpen the template with new data
                group.member_indices.push_back(i);
                placed = true;
                break; // Stop looking, it found a home
            }
        }
        
        // If it doesn't fit anywhere, spawn a new group
        if (!placed) {
            Group new_group;
            new_group.representative = input[i];
            new_group.member_indices.push_back(i);
            groups.push_back(new_group);
        }
    }

    // Post-processing: Fill any lingering wildcards in representatives with 0
    for (auto& group : groups) {
        for (auto& val : group.representative) {
            if (val == WILDCARD) {
                val = 0; 
            }
        }
    }

    // Map vector indices to group IDs for final printout
    std::vector<int> assignments(num_vectors);
    for (size_t g_id = 0; g_id < groups.size(); ++g_id) {
        for (int idx : groups[g_id].member_indices) {
            assignments[idx] = g_id;
        }
    }

    // Output Results
    std::cout << "--- Group Representatives (Fully Concrete) ---" << std::endl;
    for (size_t i = 0; i < groups.size(); ++i) {
        std::cout << "#" << i << " -> ";
        for (uint16_t val : groups[i].representative) std::cout << val << " ";
        std::endl(std::cout);
    }

    std::cout << "\n--- Vector Assignments ---" << std::endl;
    for (int i = 0; i < num_vectors; ++i) {
        std::cout << "[" << i << "] -> #" << assignments[i] << " (";
        for (uint16_t val : input[i]) {
            if (val == WILDCARD) std::cout << "? ";
            else std::cout << val << " ";
        }
        std::cout << ")\n";
    }

    return 0;
}
