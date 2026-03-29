#pragma once

#include <array>
#include <cstdint>


constexpr int kDuplicateRotationDataSize = 6;
class DuplicateRotations {
public:
    static void Initialize();
    static std::array<uint64_t, kDuplicateRotationDataSize> GetData() {
        return data;
    }

private:
    static std::array<uint64_t, kDuplicateRotationDataSize> data;
};
