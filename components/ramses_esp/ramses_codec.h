#pragma once

#include <cstdint>

namespace esphome {
namespace ramses_esp {

// Manchester encode 4-bit nibble to 8-bit little-endian byte
uint8_t manchester_encode(uint8_t nibble);

// Manchester decode 8-bit little-endian byte to 4-bit nibble
uint8_t manchester_decode(uint8_t byte);

// Validate whether a byte contains valid Manchester pairs
bool manchester_code_valid(uint8_t code);

} // namespace ramses_esp
} // namespace esphome
