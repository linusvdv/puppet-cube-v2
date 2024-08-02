#pragma once
#include <array>
#include <cstdint>


constexpr int kNumPositions = 88179840; // 8! * 3^7
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

    unsigned int GetPositionHash ();
    
    // new position resets computed data
    void SetNewPosition () {
        calculated_position_hash_ = false;
        got_legal_move_data = false;
    }

    // buffer legal move data after lookup
    bool got_legal_move_data = false;
    uint16_t legal_move_data;

private:
    // buffer hash
    bool calculated_position_hash_ = false;
    unsigned int position_hash_;
};
