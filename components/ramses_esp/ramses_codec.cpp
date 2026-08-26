#include "ramses_codec.h"

namespace esphome {
namespace ramses_esp {

// Big-endian 4 bits to little-endian byte
static const uint8_t MAN_ENCODE_TABLE[16] = {0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99,
                                             0x96, 0x95, 0x6A, 0x69, 0x66, 0x65,
                                             0x5A, 0x59, 0x56, 0x55};

// Little-endian 4 bits to 2-bit big endian
static const uint8_t MAN_DECODE_TABLE[16] = {0xF, 0xF, 0xF, 0xF, 0xF, 0x3,
                                             0x2, 0xF, 0xF, 0x1, 0x0, 0xF,
                                             0xF, 0xF, 0xF, 0xF};

uint8_t manchester_encode(uint8_t nibble) {
  return MAN_ENCODE_TABLE[nibble & 0x0F];
}

uint8_t manchester_decode(uint8_t byte) {
  uint8_t decoded = MAN_DECODE_TABLE[byte & 0x0F];
  decoded |= MAN_DECODE_TABLE[(byte >> 4) & 0x0F] << 2;
  return decoded;
}

bool manchester_code_valid(uint8_t code) {
  return (MAN_DECODE_TABLE[(code >> 4) & 0x0F] != 0x0F) &&
         (MAN_DECODE_TABLE[code & 0x0F] != 0x0F);
}

} // namespace ramses_esp
} // namespace esphome
