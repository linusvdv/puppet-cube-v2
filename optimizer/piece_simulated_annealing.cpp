#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cmath>
#include <iomanip>

using namespace std;

struct Transition {
    int src, dst;
};

// Returns the number of differing bits
int hamming(int a, int b) {
    return __builtin_popcount(a ^ b);
}

// Cost function: Penalizes non-Gray transitions (distance != 1)
long long calculate_cost(const vector<int>& pool, const vector<Transition>& transitions) {
    long long cost = 0;
    for (const auto& t : transitions) {
        int dist = hamming(pool[t.src], pool[t.dst]);
        if (dist != 1) {
            // Exponential penalty: 2-bit flip = 100, 3-bit flip = 1000, etc.
            cost += (long long)pow(10, dist);
        }
    }
    return cost;
}

int main() {
    // 1. Define Cube Geometry
    // Slots: 0:UR, 1:UF, 2:UL, 3:UB, 4:DR, 5:DF, 6:DL, 7:DB, 8:FR, 9:FL, 10:BL, 11:BR
    struct MoveDef {
        string name;
        vector<int> cycle;
        bool flips;
    };

    vector<MoveDef> moves = {
        {"U", {0, 1, 2, 3}, false},
        {"D", {4, 7, 6, 5}, false},
        {"F", {1, 9, 5, 8}, true},  // F and B flip orientation in this EO scheme
        {"B", {3, 11, 7, 10}, true},
        {"L", {2, 10, 6, 9}, false},
        {"R", {0, 8, 4, 11}, false}
    };

    vector<Transition> all_transitions;
    for (const auto& m : moves) {
        for (int i = 0; i < 4; ++i) {
            int src_slot = m.cycle[i];
            int dst_slot = m.cycle[(i + 1) % 4];
            for (int ori = 0; ori < 2; ++ori) {
                int src_state = src_slot * 2 + ori;
                int dst_ori = m.flips ? (1 - ori) : ori;
                int dst_state = dst_slot * 2 + dst_ori;
                all_transitions.push_back({src_state, dst_state});
            }
        }
    }

    // 2. Setup Mapping
    // pool[0..23] are the values for our 24 states.
    // pool[24..31] are the unused 5-bit values.
    vector<int> pool(32);
    iota(pool.begin(), pool.end(), 0);
    
    mt19937 rng(random_device{}());
    shuffle(pool.begin(), pool.end(), rng);

    long long current_cost = calculate_cost(pool, all_transitions);
    long long best_cost = current_cost;
    vector<int> best_pool = pool;

    // 3. Simulated Annealing Parameters (High Compute)
    double temp = 100.0;
    double cooling = 0.99999995; // Extremely slow cooling
    long long iterations = 500000000; // 500 Million iterations

    cout << "Starting deep optimization (500M iterations)..." << endl;

    for (long long i = 0; i < iterations; ++i) {
        // Pick one state currently in use (0-23)
        int idx1 = uniform_int_distribution<int>(0, 23)(rng);
        // Pick any value in the 32-bit pool to swap with
        int idx2 = uniform_int_distribution<int>(0, 31)(rng);
        
        swap(pool[idx1], pool[idx2]);

        long long new_cost = calculate_cost(pool, all_transitions);

        // Metropolis Acceptance Criterion
        if (new_cost <= current_cost || exp((double)(current_cost - new_cost) / temp) > uniform_real_distribution<double>(0, 1)(rng)) {
            current_cost = new_cost;
            if (current_cost < best_cost) {
                best_cost = current_cost;
                best_pool = pool;
                if (best_cost == 0) break; // Found a perfect Gray Code!
            }
        } else {
            swap(pool[idx1], pool[idx2]); // Revert
        }

        temp *= cooling;

        if (i % 25000000 == 0) {
            cout << "Iter: " << i / 1000000 << "M | Best Cost: " << best_cost << " | Temp: " << temp << endl;
        }
    }

    // 4. Final Report
    cout << "\n--- Optimization Finished ---" << endl;
    cout << "Final Best Cost: " << best_cost << endl;

    int gray = 0, two_bit = 0, bad = 0;
    for (const auto& t : all_transitions) {
        int d = hamming(best_pool[t.src], best_pool[t.dst]);
        if (d == 1) gray++;
        else if (d == 2) two_bit++;
        else bad++;
    }

    cout << "Gray (1-bit): " << gray << " | 2-bit: " << two_bit << " | 3+ bit: " << bad << endl;
    cout << "Average bits per piece move: " << (double)(gray + two_bit * 2 + bad * 3) / 48.0 << endl;

    cout << "\nMapping (State Index -> 5-bit Val):" << endl;
    for (int s = 0; s < 24; ++s) {
        cout << "State " << setw(2) << s << " (Slot " << s/2 << ", Ori " << s%2 << ") -> " << best_pool[s] << endl;
    }

    return 0;
}
