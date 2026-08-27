#pragma once

#include "struct.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace esphome {
namespace ramses_esp {

#define RAMSES_MAX_ADDR 3
#define RAMSES_MAX_PAYLOAD 64
#define RAMSES_MAX_RAW 162

enum RamsesMsgType {
  RAMSES_MSG_RQ = 0,
  RAMSES_MSG_I = 1,
  RAMSES_MSG_W = 2,
  RAMSES_MSG_RP = 3,
  RAMSES_MSG_MAX = 4
};

#define RAMSES_F_MASK 0x03
#define RAMSES_F_ADDR0 0x10
#define RAMSES_F_ADDR1 0x20
#define RAMSES_F_ADDR2 0x40
#define RAMSES_F_PARAM0 0x04
#define RAMSES_F_PARAM1 0x08
#define RAMSES_F_RSSI 0x80
#define RAMSES_F_OPCODE 0x01
#define RAMSES_F_LEN 0x02

#define HDR_T_MASK 0x30
#define HDR_T_SHIFT 4
#define HDR_A_MASK 0x0C
#define HDR_A_SHIFT 2
#define HDR_PARAM0 0x02
#define HDR_PARAM1 0x01

struct RamsesAddress {
  uint8_t dev_class{0};
  uint32_t id{0};
  bool is_valid{false};

  std::string to_string() const;
  static RamsesAddress from_bytes(const uint8_t *bytes);
  static RamsesAddress from_string(const std::string &str);
  void to_bytes(uint8_t *bytes) const;

  bool operator==(const RamsesAddress &other) const {
    return dev_class == other.dev_class && id == other.id &&
           is_valid == other.is_valid;
  }
  bool operator!=(const RamsesAddress &other) const {
    return !(*this == other);
  }
};

class RamsesMessageBuilder;

struct RamsesMessage {
  RamsesMsgType type{RAMSES_MSG_I};
  uint8_t fields{0};
  uint8_t rx_fields{0};
  uint8_t error{0};

  uint8_t addr[RAMSES_MAX_ADDR][3]{{0}};
  uint8_t param[2]{0};

  uint8_t opcode[2]{0};
  uint8_t len{0};

  uint8_t csum{0};
  uint8_t rssi{0};

  uint8_t n_payload{0};
  uint8_t payload[RAMSES_MAX_PAYLOAD]{0};

  char timestamp[36]{0};

  void reset();

  bool is_valid() const;
  uint8_t calculate_checksum() const;
  std::string to_hgi80() const;
  bool from_hgi80(const std::string &line);
  std::vector<uint8_t> to_raw_frame() const;

  static RamsesMessageBuilder builder(RamsesMsgType type = RAMSES_MSG_I);
  static RamsesMessageBuilder query();
  static RamsesMessageBuilder info();
  static RamsesMessageBuilder write();
  static RamsesMessageBuilder reply();
};

class RamsesMessageBuilder {
public:
  explicit RamsesMessageBuilder(RamsesMsgType type = RAMSES_MSG_I) {
    msg_.reset();
    this->type(type);
  }

  static RamsesMessageBuilder query() {
    return RamsesMessageBuilder(RAMSES_MSG_RQ);
  }
  static RamsesMessageBuilder info() {
    return RamsesMessageBuilder(RAMSES_MSG_I);
  }
  static RamsesMessageBuilder write() {
    return RamsesMessageBuilder(RAMSES_MSG_W);
  }
  static RamsesMessageBuilder reply() {
    return RamsesMessageBuilder(RAMSES_MSG_RP);
  }

  RamsesMessageBuilder &type(RamsesMsgType t) {
    msg_.type = t;
    msg_.fields = (msg_.fields & ~RAMSES_F_MASK) |
                  (static_cast<uint8_t>(t) & RAMSES_F_MASK);
    return *this;
  }

  RamsesMessageBuilder &from(const RamsesAddress &src) { return addr0(src); }

  RamsesMessageBuilder &to(const RamsesAddress &dst) { return addr1(dst); }

  RamsesMessageBuilder &addr0(const RamsesAddress &address) {
    return addr(0, address);
  }

  RamsesMessageBuilder &addr1(const RamsesAddress &address) {
    return addr(1, address);
  }

  RamsesMessageBuilder &addr2(const RamsesAddress &address) {
    return addr(2, address);
  }

  RamsesMessageBuilder &addr(size_t index, const RamsesAddress &address) {
    if (index < RAMSES_MAX_ADDR) {
      address.to_bytes(msg_.addr[index]);
      if (index == 0)
        msg_.fields |= RAMSES_F_ADDR0;
      else if (index == 1)
        msg_.fields |= RAMSES_F_ADDR1;
      else if (index == 2)
        msg_.fields |= RAMSES_F_ADDR2;
    }
    return *this;
  }

  RamsesMessageBuilder &opcode(uint16_t op) {
    msg_.opcode[0] = static_cast<uint8_t>((op >> 8) & 0xFF);
    msg_.opcode[1] = static_cast<uint8_t>(op & 0xFF);
    msg_.fields |= RAMSES_F_OPCODE;
    return *this;
  }

  RamsesMessageBuilder &opcode(uint8_t op0, uint8_t op1) {
    msg_.opcode[0] = op0;
    msg_.opcode[1] = op1;
    msg_.fields |= RAMSES_F_OPCODE;
    return *this;
  }

  RamsesMessageBuilder &param0(uint8_t p0) {
    msg_.param[0] = p0;
    msg_.fields |= RAMSES_F_PARAM0;
    return *this;
  }

  RamsesMessageBuilder &param1(uint8_t p1) {
    msg_.param[1] = p1;
    msg_.fields |= RAMSES_F_PARAM1;
    return *this;
  }

  RamsesMessageBuilder &payload(const uint8_t *data, size_t len) {
    size_t copy_len = std::min(len, static_cast<size_t>(RAMSES_MAX_PAYLOAD));
    if (data && copy_len > 0) {
      std::memcpy(msg_.payload, data, copy_len);
    }
    msg_.len = static_cast<uint8_t>(copy_len);
    msg_.n_payload = static_cast<uint8_t>(copy_len);
    msg_.fields |= RAMSES_F_LEN;
    return *this;
  }

  RamsesMessageBuilder &payload(std::span<const uint8_t> data) {
    return payload(data.data(), data.size());
  }

  RamsesMessageBuilder &payload(const std::vector<uint8_t> &data) {
    return payload(data.data(), data.size());
  }

  RamsesMessageBuilder &payload(std::initializer_list<uint8_t> bytes) {
    return payload(bytes.begin(), bytes.size());
  }

  RamsesMessageBuilder &payload_byte(uint8_t byte) {
    if (msg_.len < RAMSES_MAX_PAYLOAD) {
      msg_.payload[msg_.len++] = byte;
      msg_.n_payload = msg_.len;
      msg_.fields |= RAMSES_F_LEN;
    }
    return *this;
  }

  template <typename StructFmt, typename... Args>
  RamsesMessageBuilder &payload_packed(const Args &...args) {
    size_t packed_len = StructFmt::pack(msg_.payload, args...);
    msg_.len = static_cast<uint8_t>(packed_len);
    msg_.n_payload = static_cast<uint8_t>(packed_len);
    msg_.fields |= RAMSES_F_LEN;
    return *this;
  }

  RamsesMessageBuilder &append_binding(uint8_t oem_code, uint16_t op,
                                       const RamsesAddress &addr) {
    if (msg_.len + 6 <= RAMSES_MAX_PAYLOAD) {
      using BindingTupleFmt = ::esphome::ramses_esp::binary::Struct<"!BH">;
      BindingTupleFmt::pack(msg_.payload + msg_.len, oem_code, op);
      addr.to_bytes(msg_.payload + msg_.len + 3);
      msg_.len += 6;
      msg_.n_payload = msg_.len;
      msg_.fields |= RAMSES_F_LEN;
    }
    return *this;
  }

  RamsesMessageBuilder &fields(uint8_t f) {
    msg_.fields |= f;
    return *this;
  }

  RamsesMessageBuilder &rssi(uint8_t r) {
    msg_.rssi = r;
    msg_.fields |= RAMSES_F_RSSI;
    return *this;
  }

  RamsesMessageBuilder &timestamp(const std::string &ts) {
    snprintf(msg_.timestamp, sizeof(msg_.timestamp), "%s", ts.c_str());
    return *this;
  }

  RamsesMessage build() const {
    RamsesMessage result = msg_;
    result.csum = result.calculate_checksum();
    return result;
  }

  operator RamsesMessage() const { return build(); }

private:
  RamsesMessage msg_{};
};

inline RamsesMessageBuilder RamsesMessage::builder(RamsesMsgType type) {
  return RamsesMessageBuilder(type);
}
inline RamsesMessageBuilder RamsesMessage::query() {
  return RamsesMessageBuilder::query();
}
inline RamsesMessageBuilder RamsesMessage::info() {
  return RamsesMessageBuilder::info();
}
inline RamsesMessageBuilder RamsesMessage::write() {
  return RamsesMessageBuilder::write();
}
inline RamsesMessageBuilder RamsesMessage::reply() {
  return RamsesMessageBuilder::reply();
}

uint8_t ramses_decode_header(uint8_t header);
uint8_t ramses_encode_header(uint8_t fields);

} // namespace ramses_esp
} // namespace esphome
