#pragma once
#include <cstdint>


namespace edge {
    void Init();
    void Rotate(uint32_t& pos, uint8_t& sym, uint16_t& orient, uint8_t rot);
}
