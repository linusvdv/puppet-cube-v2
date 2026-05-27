#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

const uint16_t WILDCARD = 65535;
const int VECTOR_SIZE = 48;

struct Element {
    std::vector<uint16_t> data;
    int original_index;
    int wildcard_count;
};

struct Group {
    std::vector<uint16_t> representative;
    std::vector<int> member_indices;
};

// Checks compatibility and returns a potential merged representative
bool try_merge(const std::vector<uint16_t>& rep, const std::vector<uint16_t>& vec, std::vector<uint16_t>& merged) {
    for (size_t i = 0; i < rep.size(); ++i) {
        if (rep[i] == WILDCARD) {
            merged[i] = vec[i];
        } else if (vec[i] == WILDCARD) {
            merged[i] = rep[i];
        } else if (rep[i] != vec[i]) {
            return false;
        } else {
            merged[i] = rep[i];
        }
    }
    return true;
}

// Counts how many wildcards are in a vector
int count_wildcards(const std::vector<uint16_t>& vec) {
    int count = 0;
    for (uint16_t val : vec) {
        if (val == WILDCARD) count++;
    }
    return count;
}

// Evaluates a specific permutation of elements and returns the resulting groups
std::vector<Group> run_greedy_pass(const std::vector<Element>& elements) {
    std::vector<Group> groups;
    std::vector<uint16_t> merged_rep(VECTOR_SIZE);

    for (const auto& elem : elements) {
        bool placed = false;
        
        // Find the first compatible group
        for (auto& group : groups) {
            if (try_merge(group.representative, elem.data, merged_rep)) {
                group.representative = merged_rep;
                group.member_indices.push_back(elem.original_index);
                placed = true;
                break;
            }
        }
        
        if (!placed) {
            Group new_group;
            new_group.representative = elem.data;
            new_group.member_indices.push_back(elem.original_index);
            groups.push_back(new_group);
        }
    }
    return groups;
}

int main() {
    // 1. Mock/Load data setup (Replace this with your 2861 real elements)
    std::vector<std::vector<uint16_t>> raw_input;
    for (int i = 0; i < 2861; i++) {
        std::vector<uint16_t> nums(48);
        for (uint16_t& num : nums) {
            std::cin >> num;
        }
        raw_input.push_back(nums);
    }

    int num_vectors = raw_input.size();
    std::vector<Element> elements(num_vectors);
    for (int i = 0; i < num_vectors; ++i) {
        elements[i].data = raw_input[i];
        elements[i].original_index = i;
        elements[i].wildcard_count = count_wildcards(raw_input[i]);
    }

    // 2. Initial sort strategy: Least wildcards (most strict constraints) first
    std::sort(elements.begin(), elements.end(), [](const Element& a, const Element& b) {
        return a.wildcard_count < b.wildcard_count; 
    });

    // Run baseline check
    std::vector<Group> best_groups = run_greedy_pass(elements);
    std::cout << "Baseline Group Count: " << best_groups.size() << std::endl;

    // 3. Monte Carlo / Randomized Shuffling Optimization Loop
    std::random_device rd;
    std::mt19937 g(rd());
    
    // Allow the algorithm to search for a maximum of 4.0 seconds
    auto start_time = std::chrono::high_resolution_clock::now();
    double time_limit_seconds = 4.0; 
    
    int iterations = 0;
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_time - start_time;
        if (elapsed.count() >= time_limit_seconds) {
            break;
        }

        // Keep the top 10% most rigid vectors sorted at the front, shuffle the rest
        int freeze_boundary = num_vectors / 10;
        std::shuffle(elements.begin() + freeze_boundary, elements.end(), g);

        std::vector<Group> current_groups = run_greedy_pass(elements);
        
        // If we found a configuration with fewer groups, save it!
        if (current_groups.size() < best_groups.size()) {
            best_groups = current_groups;
            std::cout << "Found better configuration! Group count reduced to: " << best_groups.size() << std::endl;
            
            // If we hit a satisfactory hard-target limit (e.g., well under 256), we can choose to exit early
            if (best_groups.size() <= 256) {
                // optional: break;
            }
        }
        iterations++;
    }

    std::cout << "\nOptimization finished after " << iterations << " rounds." << std::endl;
    std::cout << "Final Optimal Group Count: " << best_groups.size() << std::endl;

    // 4. Fill lingering wildcards in the best representative results
    for (auto& group : best_groups) {
        for (auto& val : group.representative) {
            if (val == WILDCARD) val = 0; 
        }
    }

    // Generate flat assignments
    std::vector<int> assignments(num_vectors);
    for (size_t g_id = 0; g_id < best_groups.size(); ++g_id) {
        for (int idx : best_groups[g_id].member_indices) {
            assignments[idx] = g_id;
        }
    }

    // Output clean summary
    std::cout << "\n--- Optimized Group Representatives ---" << std::endl;
    for (size_t i = 0; i < std::min(best_groups.size(), (size_t)10); ++i) { // showing first 10 for brevety
        std::cout << "#" << i << " (Size: " << best_groups[i].member_indices.size() << ") -> ";
        for (uint16_t val : best_groups[i].representative) std::cout << val << " ";
        std::cout << "\n";
    }
    if (best_groups.size() > 10) std::cout << "... and " << (best_groups.size() - 10) << " more groups.\n";

    return 0;
}
