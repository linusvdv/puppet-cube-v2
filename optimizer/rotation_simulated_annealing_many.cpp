#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cmath>
#include <iomanip>

using namespace std;

struct PieceTransition { int src_state, dst_state; };
struct MoveGroup { string name; vector<PieceTransition> transitions; };

int hamming(int a, int b) { return __builtin_popcount(a ^ b); }

long long calculate_rotation_cost(const vector<int>& pool, const vector<MoveGroup>& groups) {
    long long total_penalty = 0;
    int global_max_bits = 0;
    for (const auto& g : groups) {
        for (int combo = 0; combo < 16; ++combo) {
            int bits = 0;
            for (int p = 0; p < 4; ++p) {
                const auto& t = g.transitions[p * 2 + ((combo >> p) & 1)];
                bits += hamming(pool[t.src_state], pool[t.dst_state]);
            }
            total_penalty += (long long)pow(bits, 2); 
            if (bits > global_max_bits) global_max_bits = bits;
        }
    }
    total_penalty += (long long)pow(global_max_bits, 4);
    return total_penalty;
}


#include <fstream>

// Call this at the end of your main() function
void export_tables(const vector<int>& pool, const vector<MoveGroup>& groups) {
    // 3. Export all 18 tables
    ofstream out("tables.txt");
    // Write mapping first...
    for (int i = 0; i < 24; ++i) out << pool[i] << (i == 23 ? "" : " ");
    out << "\n";

    for (const auto& g : groups) {
        vector<int> table(32);
        for (int i = 0; i < 32; ++i) table[i] = i; 
        for (const auto& t : g.transitions) table[pool[t.src_state]] = pool[t.dst_state];
        for (int i = 0; i < 32; ++i) out << table[i] << (i == 31 ? "" : " ");
        out << "\n";
    }
}


int main() {
    // 1. Setup Move Groups
        struct MoveDef { 
        string name; 
        vector<int> cycle; 
        bool flips; 
        bool is_inverse; // Added to distinguish CCW moves
    };

    // 1. Define the 9 base moves
    vector<MoveDef> base_defs = {
        {"U", {0, 1, 2, 3}, false}, {"D", {4, 7, 6, 5}, false},
        {"F", {1, 9, 5, 8}, true},  {"B", {3, 11, 7, 10}, true},
        {"L", {2, 10, 6, 9}, false}, {"R", {0, 8, 4, 11}, false},
        {"M", {1, 5, 7, 3}, true},  {"E", {8, 10, 11, 9}, false}, {"S", {0, 2, 4, 6}, true}
    };

    // 2. Generate all 18 moves (Forward and Inverse)
    vector<MoveGroup> groups;
    for (auto& d : base_defs) {
        // Forward Move
        MoveGroup fwd; fwd.name = d.name;
        // Inverse Move
        MoveGroup inv; inv.name = d.name + "'";

        for (int i = 0; i < 4; ++i) {
            int src = d.cycle[i];
            int dst = d.cycle[(i + 1) % 4];
            for (int ori = 0; ori < 2; ++ori) {
                int next_ori = d.flips ? 1 - ori : ori;
                fwd.transitions.push_back({src * 2 + ori, dst * 2 + next_ori});
                // Inverse is just the swap of src and dst
                inv.transitions.push_back({dst * 2 + next_ori, src * 2 + ori});
            }
        }
        groups.push_back(fwd);
        groups.push_back(inv);
    }


    // 2. Initialize Mapping
    mt19937 rng(random_device{}());
    
    // Multi-start parameters
    int num_starts = 1;
    // long long iterations_per_start = 30000000; // 10M per run
    long long iterations_per_start = 3; // 10M per run
    
    vector<int> best_overall_pool;
    long long best_overall_cost = -1;

    for (int run = 0; run < num_starts; ++run) {
        vector<int> pool(32); iota(pool.begin(), pool.end(), 0);
        shuffle(pool.begin(), pool.end(), rng);

        // We reset temperature high for every fresh start
        double temp = 5000.0;
        double cooling = 0.99999975;
        long long current_cost = calculate_rotation_cost(pool, groups);

        for (long long i = 0; i < iterations_per_start; ++i) {
            int idx1 = uniform_int_distribution<int>(0, 23)(rng);
            int idx2 = uniform_int_distribution<int>(0, 31)(rng);
            swap(pool[idx1], pool[idx2]);
            
            long long new_cost = calculate_rotation_cost(pool, groups);
            if (new_cost <= current_cost || exp((double)(current_cost - new_cost) / temp) > uniform_real_distribution<double>(0, 1)(rng)) {
                current_cost = new_cost;
            } else {
                swap(pool[idx1], pool[idx2]);
            }
            temp *= cooling;
            if (i % 1000000 == 0) {
                cout << "#" << i/1000000 << "M: temp " << temp << " cost " << current_cost << endl;
            }
        }

        if (best_overall_cost == -1 || current_cost < best_overall_cost) {
            best_overall_cost = current_cost;
            best_overall_pool = pool;
            cout << "Run " << run << " completed. New Global Best Cost: " << best_overall_cost << endl;
        } else {
            cout << "Run " << run << " completed. (No improvement: " << current_cost << ")" << endl;
        }
    }

    cout << "\n--- FINAL ROTATION REPORT ---" << endl;
    int absolute_max = 0;
    double total_bits = 0;
    vector<int> rotation_counts(12, 0); // 0-11 bits possible

    for (const auto& g : groups) {
        int move_max = 0;
        for (int combo = 0; combo < 16; ++combo) {
            int bits = 0;
            for (int p = 0; p < 4; ++p) {
                bits += hamming(best_overall_pool[g.transitions[p*2 + ((combo>>p)&1)].src_state], 
                                best_overall_pool[g.transitions[p*2 + ((combo>>p)&1)].dst_state]);
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
        cout << "State " << setw(2) << s << " (Slot " << s/2 << ", Ori " << s%2 << ") -> " << best_overall_pool[s] << endl;
    }

    export_tables(best_overall_pool, groups);

    return 0;
}
