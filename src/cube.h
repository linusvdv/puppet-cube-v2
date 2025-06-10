#include <cstdint>
#include <vector>


constexpr int kNumCornerData = 88179840;  // 8! * 3^7
constexpr int kNumEdgeData = 42577920;  // 12! / 6! * 2^6
constexpr int kNumRotations = 18;
constexpr int kNumCorners = 8;


enum Rotations : uint8_t {
    kR,
    kRc,
    kL,
    kLc,
    kU,
    kUc,
    kD,
    kDc,
    kF,
    kFc,
    kB,
    kBc,
    kM,
    kMc,
    kE,
    kEc,
    kS,
    kSc
};


class Cube {
public:
    // corner and edge precomputation
    static void Initialize();

private:
    static std::vector<uint8_t> corner_orientation;
    static std::vector<uint8_t> corner_position;
};
