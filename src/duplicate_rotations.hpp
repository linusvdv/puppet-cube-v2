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
    static bool IsDuplicate(const uint8_t& last, const uint8_t& current);

private:
    static std::array<uint64_t, kDuplicateRotationDataSize> data;
};
