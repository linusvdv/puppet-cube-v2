#pragma once
#include <cstdint>


namespace edge {
    void Init();
    void Rotate(uint32_t& position, uint8_t& symmetry, uint16_t& orientation, uint8_t rotation);
}
