// cudd_pdb_example.cpp
//
// Build an ADD from the edge heuristic PDB and measure memory usage.
//
// Compile:
//   g++ -O2 -std=c++17 cudd_pdb_example.cpp -lcudd -o cudd_pdb_example
//   (or with vcpkg/brew: adjust include/lib paths as needed)

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include "cudd.h"

// ── Constants matching your codebase ────────────────────────────────────────
constexpr int    kOriBits   = 11;                   // 2^11 = 2048 orientations
constexpr int    kPosBits   = 20;                   // 2^20 = 1048576 >= 665280 positions
constexpr int    kTotalBits = kOriBits + kPosBits;  // 31 ADD variables total

constexpr size_t kNumEdgeOrientation = 1 << kOriBits;   //    2,048
constexpr size_t kNumEdgePositions   = 665280;           //  665,280  (12!/6!)
constexpr size_t kNumEdgeHeuristic   = kNumEdgeOrientation * kNumEdgePositions;
//                                                        1,362,493,440 entries

// ── Early-exit check: is every byte in data[offset .. offset+size) the same?
// If so, we can create a single terminal node instead of recursing.
static bool IsUniformBlock(const std::vector<uint8_t>& data,
                            size_t offset, size_t size, uint8_t& out_val)
{
    // Indices past the end of the real data are treated as 0
    if (offset >= data.size()) { out_val = 0; return true; }

    const size_t end = std::min(offset + size, data.size());
    out_val = data[offset];
    for (size_t i = offset + 1; i < end; ++i)
        if (data[i] != out_val) return false;

    // Any padding beyond data.size() is 0; if the real block wasn't all-zero it
    // is still non-uniform.
    if (offset + size > data.size() && out_val != 0) return false;
    return true;
}

// ── Recursively build an ADD for data[offset .. offset + 2^num_vars) ─────────
//
//  var      – CUDD variable index for the current (most-significant) bit
//  num_vars – how many bits/variables remain below this level
//  offset   – start of this block in the flat data array
//
//  Returns a referenced DdNode*.  Caller must Cudd_RecursiveDeref when done.
DdNode* BuildADD(DdManager* mgr,
                 const std::vector<uint8_t>& data,
                 int    var,
                 int    num_vars,
                 size_t offset)
{
    const size_t block_size = (size_t)1 << num_vars;

    // ── Terminal node ────────────────────────────────────────────────────────
    if (num_vars == 0) {
        uint8_t val = (offset < data.size()) ? data[offset] : 0;
        DdNode* node = Cudd_addConst(mgr, static_cast<double>(val));
        Cudd_Ref(node);
        return node;
    }

    // ── Uniform-block optimisation ───────────────────────────────────────────
    // If the entire block holds one value we can skip all deeper recursion.
    uint8_t uniform_val = 0;
    if (IsUniformBlock(data, offset, block_size, uniform_val)) {
        DdNode* node = Cudd_addConst(mgr, static_cast<double>(uniform_val));
        Cudd_Ref(node);
        return node;
    }

    // ── Recurse on the two halves, split on the current variable ─────────────
    const size_t half = block_size >> 1;

    // var == 0  →  "lo" branch is taken when var = 0 (left/lower block)
    DdNode* lo = BuildADD(mgr, data, var + 1, num_vars - 1, offset);
    Cudd_Ref(lo);
    DdNode* hi = BuildADD(mgr, data, var + 1, num_vars - 1, offset + half);
    Cudd_Ref(hi);

    DdNode* result;
    if (lo == hi) {
        // Both halves collapsed to the same node – no need for a new ITE node.
        result = lo;
        // lo already holds one Ref; we must release the extra hi ref
        Cudd_RecursiveDeref(mgr, hi);
        return result;   // still referenced by the surviving lo ref
    }

    DdNode* v = Cudd_addIthVar(mgr, var);
    Cudd_Ref(v);
    // Cudd_addIte(mgr, f, g, h) returns  f=1 → g,  f=0 → h
    result = Cudd_addIte(mgr, v, hi, lo);
    Cudd_Ref(result);

    Cudd_RecursiveDeref(mgr, v);
    Cudd_RecursiveDeref(mgr, lo);
    Cudd_RecursiveDeref(mgr, hi);
    return result;
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static void PrintSeparator() { std::cout << std::string(50, '-') << '\n'; }

template<typename T>
static std::string HumanBytes(T bytes_in)
{
    double b = static_cast<double>(bytes_in);
    const char* units[] = {"B","KB","MB","GB"};
    int u = 0;
    while (b >= 1024.0 && u < 3) { b /= 1024.0; ++u; }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f %s", b, units[u]);
    return buf;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main()
{
    // 1. Load binary PDB ──────────────────────────────────────────────────────
    std::cout << "Loading PDB from file...\n";
    std::vector<uint8_t> edge_heuristics(kNumEdgeHeuristic);

    std::ifstream file("../precomputation/edge_heuristics.bin", std::ios::binary);
    if (!file) { std::cerr << "Could not open file\n"; return 1; }
    file.read(reinterpret_cast<char*>(edge_heuristics.data()),
              static_cast<std::streamsize>(kNumEdgeHeuristic));
    std::cout << "Loaded " << kNumEdgeHeuristic << " entries ("
              << HumanBytes(kNumEdgeHeuristic) << " raw)\n\n";

    // 2. Initialise CUDD ───────────────────────────────────────────────────────
    //    Third arg: initial unique-table slots (power of 2).
    //    Fourth arg: initial cache slots.
    //    Fifth arg:  max memory CUDD may use in bytes (0 = unlimited).
    DdManager* mgr = Cudd_Init(kTotalBits, 0,
                                CUDD_UNIQUE_SLOTS,
                                CUDD_CACHE_SLOTS,
                                0);
    if (!mgr) { std::cerr << "Cudd_Init failed\n"; return 1; }

    // Optional: allow CUDD to dynamically reorder variables for better compression
    // Cudd_AutodynEnable(mgr, CUDD_REORDER_SIFT);

    // 3. Build ADD ─────────────────────────────────────────────────────────────
    std::cout << "Building ADD with " << kTotalBits << " variables...\n";
    std::cout << "  vars  0-10 → edge orientation (11 bits)\n";
    std::cout << "  vars 11-30 → edge position    (20 bits, "
              << (1 << kPosBits) - kNumEdgePositions << " indices padded to 0)\n";
    PrintSeparator();

    auto t0 = std::chrono::steady_clock::now();

    // The flat index layout:  ori * kNumEdgePositions + pos
    // We encode ori in the upper kOriBits variables and pos in the lower kPosBits,
    // but the flat array is NOT a power-of-2 stride, so we must pad pos-space to
    // 2^kPosBits.  We do this transparently in IsUniformBlock / BuildADD by
    // treating out-of-range indices as 0.
    //
    // To make the flat-array index line up with the binary variable layout we
    // rebuild the vector in the "padded" layout: ori * 2^kPosBits + pos.
    const size_t kPaddedSize = (size_t)kNumEdgeOrientation << kPosBits;
    std::vector<uint8_t> padded(kPaddedSize, 0);
    for (size_t ori = 0; ori < kNumEdgeOrientation; ++ori)
        for (size_t pos = 0; pos < kNumEdgePositions; ++pos)
            padded[ori * (1 << kPosBits) + pos] =
                edge_heuristics[ori * kNumEdgePositions + pos];

    DdNode* add = BuildADD(mgr, padded, 0, kTotalBits, 0);
    // add is already Cudd_Ref'd by BuildADD

    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    // 4. Print statistics ──────────────────────────────────────────────────────
    PrintSeparator();
    std::cout << "ADD built in " << elapsed << " s\n\n";

    long   dag_nodes  = Cudd_DagSize(add);          // nodes reachable from add
    long   total_nodes= Cudd_ReadNodeCount(mgr);     // all live nodes in manager
    long   peak_nodes = Cudd_ReadPeakNodeCount(mgr); // peak since init
    long   mem_bytes  = Cudd_ReadMemoryInUse(mgr);   // bytes used by CUDD

    std::cout << "=== ADD Statistics ===\n";
    std::cout << "  Reachable nodes (DAG size) : " << dag_nodes   << "\n";
    std::cout << "  Total live nodes in manager: " << total_nodes << "\n";
    std::cout << "  Peak node count            : " << peak_nodes  << "\n";
    std::cout << "  CUDD memory in use         : " << HumanBytes(mem_bytes) << "\n";

    // Each CUDD node is ~40 bytes on a 64-bit system (index, two children, ref-count, next)
    long est_node_bytes = dag_nodes * 40L;
    std::cout << "  Est. node storage (40 B ea): " << HumanBytes(est_node_bytes) << "\n";

    PrintSeparator();
    size_t raw_bytes = kNumEdgeHeuristic * sizeof(uint8_t);
    std::cout << "=== Compression Comparison ===\n";
    std::cout << "  Raw table size : " << HumanBytes(raw_bytes)    << "\n";
    std::cout << "  CUDD mem usage : " << HumanBytes(mem_bytes)    << "\n";
    std::cout << "  Ratio          : "
              << static_cast<double>(mem_bytes) / raw_bytes << "x\n";

    // 5. Sanity check: query a few entries ────────────────────────────────────
    // To evaluate the ADD at a specific (ori, pos) point, set variable assignments.
    // Here we check entry (ori=0, pos=0).
    {
        int* vars = new int[kTotalBits]();  // all 0 → ori=0, pos=0
        DdNode* leaf = Cudd_Eval(mgr, add, vars);
        std::cout << "\nADD[ori=0, pos=0] = "
                  << static_cast<int>(Cudd_V(leaf))
                  << "  (expected: " << (int)edge_heuristics[0] << ")\n";
        delete[] vars;
    }

    // 6. Cleanup ──────────────────────────────────────────────────────────────
    Cudd_RecursiveDeref(mgr, add);
    int check = Cudd_CheckZeroRef(mgr);   // should print 0 if no leaks
    std::cout << "Leaked references: " << check << "\n";
    Cudd_Quit(mgr);
    return 0;
}
