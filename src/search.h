#pragma once

class Search {
    static void Initialize();

private:
    static int* d_corner_heuristics;
    static int* d_edge_heuristics;
};
