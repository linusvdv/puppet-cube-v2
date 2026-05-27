#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cmath>
#include <iomanip>

using namespace std;

struct PieceTransition {
    int src_state, dst_state;
};

struct MoveGroup {
    string name;
    vector<PieceTransition> transitions; // 8 transitions (4 slots * 2 oris)
};

int hamming(int a, int b) {
    return __builtin_popcount(a ^ b);
}

// NEW COST FUNCTION: Evaluates rotations as whole units
long long calculate_rotation_cost(const vector<int>& pool, const vector<MoveGroup>& groups) {
    long long total_penalty = 0;
    int global_max_bits = 0;

    for (const auto& g : groups) {
        // Each move has 4 piece-positions. Each piece has 2 possible orientations.
        // We check all 16 orientation combinations for this face.
        for (int combo = 0; combo < 16; ++combo) {
            int rotation_bits = 0;
            for (int p = 0; p < 4; ++p) {
                int ori = (combo >> p) & 1;
                // Get the transition for this piece at this orientation
                // transitions[p*2] is ori 0, transitions[p*2 + 1] is ori 1
                const auto& t = g.transitions[p * 2 + ori];
                rotation_bits += hamming(pool[t.src_state], pool[t.dst_state]);
            }
            
            // Penalize high bit-change rotations exponentially
            // A 7 or 8 bit rotation is much worse than a 4 or 5 bit one.
            total_penalty += (long long)pow(rotation_bits, 4); 
            
            if (rotation_bits > global_max_bits) global_max_bits = rotation_bits;
        }
    }
    // Add a massive penalty for the absolute worst-case rotation to force it down
    total_penalty += (long long)pow(global_max_bits, 8);
    return total_penalty;
}

int main() {
    // 1. Setup Move Groups
    struct MoveDef { string name; vector<int> cycle; bool flips; };
    vector<MoveDef> defs = {
        {"U", {0, 1, 2, 3}, false}, {"D", {4, 7, 6, 5}, false},
        {"F", {1, 9, 5, 8}, true},  {"B", {3, 11, 7, 10}, true},
        {"L", {2, 10, 6, 9}, false}, {"R", {0, 8, 4, 11}, false}
    };

    vector<MoveGroup> groups;
    for (const auto& d : defs) {
        MoveGroup mg; mg.name = d.name;
        for (int i = 0; i < 4; ++i) {
            int src_slot = d.cycle[i], dst_slot = d.cycle[(i + 1) % 4];
            for (int ori = 0; ori < 2; ++ori) {
                mg.transitions.push_back({src_slot * 2 + ori, dst_slot * 2 + (d.flips ? 1 - ori : ori)});
            }
        }
        groups.push_back(mg);
    }

    // 2. Initialize Mapping
    vector<int> pool(32); iota(pool.begin(), pool.end(), 0);
    mt19937 rng(random_device{}());
    shuffle(pool.begin(), pool.end(), rng);

    long long current_cost = calculate_rotation_cost(pool, groups);
    long long best_cost = current_cost;
    vector<int> best_pool = pool;

    // 3. Optimization Parameters
    double temp = 200000;
    double cooling = 0.9999995;
    long long iterations = 40000000;

    cout << "Optimizing for Rotation Bit-Change (200M iterations)..." << endl;

    for (long long i = 0; i < iterations; ++i) {
        int idx1 = uniform_int_distribution<int>(0, 23)(rng);
        int idx2 = uniform_int_distribution<int>(0, 31)(rng);
        
        swap(pool[idx1], pool[idx2]);
        long long new_cost = calculate_rotation_cost(pool, groups);

        if (new_cost <= current_cost || exp((double)(current_cost - new_cost) / temp) > uniform_real_distribution<double>(0, 1)(rng)) {
            current_cost = new_cost;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_pool = pool;
            }
        } else {
            swap(pool[idx1], pool[idx2]);
        }

        if (i % 100000 == 0) {
            temp *= 0.95; // Manual step-cooling for better convergence
            cout << "Iter: " << i/1000000 << "M | Temp: " << temp << " | Best Cost: " << best_cost << endl;
        }
    }

    // 4. Final Analysis Report
    cout << "\n--- FINAL ROTATION REPORT ---" << endl;
    int absolute_max = 0;
    double total_bits = 0;
    vector<int> rotation_counts(12, 0); // 0-11 bits possible

    for (const auto& g : groups) {
        int move_max = 0;
        for (int combo = 0; combo < 16; ++combo) {
            int bits = 0;
            for (int p = 0; p < 4; ++p) {
                bits += hamming(best_pool[g.transitions[p*2 + ((combo>>p)&1)].src_state], 
                                best_pool[g.transitions[p*2 + ((combo>>p)&1)].dst_state]);
            }
            rotation_counts[bits]++;
            total_bits += bits;
            if (bits > absolute_max) absolute_max = bits;
            if (bits > move_max) move_max = bits;
        }
        cout << "Move " << g.name << " | Max Bit Change: " << move_max << endl;
    }

    cout << "\nDistribution of Rotation Costs (96 scenarios):" << endl;
    for (int i = 1; i < 12; ++i) {
        if (rotation_counts[i] > 0) cout << i << " bits: " << rotation_counts[i] << " times" << endl;
    }

    cout << "\nSTATISTICS:" << endl;
    cout << "Worst-Case Rotation: " << absolute_max << " bits" << endl;
    cout << "Average Rotation:    " << total_bits / 96.0 << " bits" << endl;

    cout << "\nMapping (State Index -> 5-bit Val):" << endl;
    for (int s = 0; s < 24; ++s) {
        cout << "State " << setw(2) << s << " (Slot " << s/2 << ", Ori " << s%2 << ") -> " << best_pool[s] << endl;
    }

    return 0;
}
