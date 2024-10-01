#pragma once
#include <array>
#include <cstdint>


#include "error_handler.h"
#include "settings.h"


// read file of legal moves and huristic funciton
void InitializePositionData (ErrorHandler& error_handler, Setting& settings);

void InitializeEdgeData (ErrorHandler& error_handler, Setting& settings);


constexpr int kNumPositions = 88179840; // 8! * 3^7
constexpr int kNumEdgePositions = 42577920; // fac(12) / fac(6) * 2^6
constexpr int kEightFac = 40320; // 8!


class Cube {
public:
    Cube();

    struct Piece {
        // where is the piece located
        // index - position in 3D space (x y z)
        //
        // corners:
        // 0 -  1  1  1
        // 1 - -1  1  1
        // 2 -  1 -1  1
        // 3 - -1 -1  1
        // ...
        // 7 - -1 -1 -1
        //
        // edges:
        // 0 -
        // ...
        // 11 -
        uint8_t position;

        // distinguish the different orientations
        // 0th bit - x direction
        // 1st bit - y direction
        // 2nd bit - z direction
        uint8_t orientation = 0;
    };

    // corners
    static const unsigned int kNumCorners = 8;
    std::array<Piece, kNumCorners> corners;

    // edges
    static const unsigned int kNumEdges = 12;
    std::array<Piece, kNumEdges> edges;

    // get hash of position
    unsigned int GetCornerHash ();
    uint64_t GetEdgeHash ();
    
    // new position resets computed data
    void SetNewPosition () {
        calculated_corner_hash_ = false;
        got_position_data = false;
        calculated_edge_hash_ = false;
        calculated_edge_heuristic_ = false;
    }

    // buffer legal move data after lookup
    bool got_position_data = false;
    uint16_t position_data;
    uint16_t GetPositionData ();

    // check if this position is a solved position
    bool IsSolved ();

    // get the heuristic function
    static const int kHeuristicFunctionOffset = 6;
    int GetHeuristicFunction () {
        return GetPositionData() >> kHeuristicFunctionOffset;
    }

    uint8_t GetEdgeHeuristic ();


private:
    // buffer hash
    bool calculated_corner_hash_ = false;
    unsigned int corner_hash_;
    bool calculated_edge_hash_ = false;
    uint64_t edge_hash_;
    bool calculated_edge_heuristic_ = false;
    uint8_t edge_heuristic_;
};


// get cube from hash
Cube DecodeHash (unsigned int corner_hash, uint64_t edge_hash);
