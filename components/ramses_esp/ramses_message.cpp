#include "ramses_message.h"
#include "ramses_codec.h"

#if __has_include("esphome/core/log.h")
#include "esphome/core/log.h"
#else
#include <cstdio>
#define ESP_LOGD(tag, ...)
#define ESP_LOGI(tag, ...)
#define ESP_LOGW(tag, ...)
#define ESP_LOGE(tag, ...)
#endif
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/time.h>

static const char *TAG = "ramses_esp.msg";

namespace esphome {
namespace ramses_esp {

static const char *const MSG_TYPE_NAMES[RAMSES_MSG_MAX] = {"RQ", "I", "W",
                                                           "RP"};

static const uint8_t ADDRESS_FLAGS[4] = {
    RAMSES_F_ADDR0 + RAMSES_F_ADDR1 + RAMSES_F_ADDR2, RAMSES_F_ADDR2,
    RAMSES_F_ADDR0 + RAMSES_F_ADDR2, RAMSES_F_ADDR0 + RAMSES_F_ADDR1};

#define HDR_T_MASK 0x30
#define HDR_T_SHIFT 4
#define HDR_A_MASK 0x0C
#define HDR_A_SHIFT 2
#define HDR_PARAM0 0x02
#define HDR_PARAM1 0x01

uint8_t ramses_decode_header(uint8_t header) {
  uint8_t fields = (header & HDR_T_MASK) >> HDR_T_SHIFT;
  fields |= ADDRESS_FLAGS[(header & HDR_A_MASK) >> HDR_A_SHIFT];
  if (header & HDR_PARAM0)
    fields |= RAMSES_F_PARAM0;
  if (header & HDR_PARAM1)
    fields |= RAMSES_F_PARAM1;
  return fields;
}

uint8_t ramses_encode_header(uint8_t fields) {
  uint8_t addresses =
      fields & (RAMSES_F_ADDR0 + RAMSES_F_ADDR1 + RAMSES_F_ADDR2);
  for (uint8_t i = 0; i < sizeof(ADDRESS_FLAGS); i++) {
    if (addresses == ADDRESS_FLAGS[i]) {
      uint8_t header = i << HDR_A_SHIFT;
      header |= (fields & RAMSES_F_MASK) << HDR_T_SHIFT;
      if (fields & RAMSES_F_PARAM0)
        header |= HDR_PARAM0;
      if (fields & RAMSES_F_PARAM1)
        header |= HDR_PARAM1;
      return header;
    }
  }
  return 0xFF;
}

std::string RamsesAddress::to_string() const {
  if (!this->is_valid)
    return "--:------";
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%06lu", (unsigned)this->dev_class,
           (unsigned long)this->id);
  return std::string(buf);
}

RamsesAddress RamsesAddress::from_bytes(const uint8_t *bytes) {
  if (bytes == nullptr)
    return RamsesAddress{};
  return RamsesAddress{
      .dev_class = static_cast<uint8_t>((bytes[0] >> 2) & 0x3F),
      .id = ((uint32_t)(bytes[0] & 0x03) << 16) | ((uint32_t)bytes[1] << 8) |
            (uint32_t)bytes[2],
      .is_valid = true,
  };
}

RamsesAddress RamsesAddress::from_string(const std::string &str) {
  if (str == "--:------" || str.length() < 9)
    return RamsesAddress{};
  unsigned dev_class = 0;
  unsigned long id = 0;
  if (sscanf(str.c_str(), "%u:%lu", &dev_class, &id) == 2) {
    return RamsesAddress{
        .dev_class = static_cast<uint8_t>(dev_class),
        .id = static_cast<uint32_t>(id),
        .is_valid = true,
    };
  }
  return RamsesAddress{};
}

void RamsesAddress::to_bytes(uint8_t *bytes) const {
  if (bytes == nullptr)
    return;
  bytes[0] = ((this->dev_class << 2) & 0xFC) | ((this->id >> 16) & 0x03);
  bytes[1] = (this->id >> 8) & 0xFF;
  bytes[2] = this->id & 0xFF;
}

void RamsesMessage::reset() {
  this->type = RAMSES_MSG_I;
  this->fields = 0;
  this->rx_fields = 0;
  this->error = 0;
  memset(this->addr, 0, sizeof(this->addr));
  memset(this->param, 0, sizeof(this->param));
  memset(this->opcode, 0, sizeof(this->opcode));
  this->len = 0;
  this->csum = 0;
  this->rssi = 0;
  this->n_payload = 0;
  memset(this->payload, 0, sizeof(this->payload));
  this->timestamp.clear();
}

uint8_t RamsesMessage::calculate_checksum() const {
  uint8_t sum = ramses_encode_header(this->fields);
  for (uint8_t i = 0; i < RAMSES_MAX_ADDR; i++) {
    if (this->fields & (RAMSES_F_ADDR0 << i)) {
      sum += this->addr[i][0] + this->addr[i][1] + this->addr[i][2];
    }
  }
  if (this->fields & RAMSES_F_PARAM0)
    sum += this->param[0];
  if (this->fields & RAMSES_F_PARAM1)
    sum += this->param[1];
  sum += this->opcode[0] + this->opcode[1];
  sum += this->len;
  for (uint8_t i = 0; i < this->len; i++) {
    sum += this->payload[i];
  }
  return static_cast<uint8_t>(256 - (sum & 0xFF));
}

bool RamsesMessage::is_valid() const {
  if (this->error != 0)
    return false;
  if ((this->rx_fields & (RAMSES_F_OPCODE | RAMSES_F_LEN)) !=
      (RAMSES_F_OPCODE | RAMSES_F_LEN))
    return false;
  if (this->n_payload != this->len)
    return false;

  uint8_t sum = ramses_encode_header(this->fields);
  for (uint8_t i = 0; i < RAMSES_MAX_ADDR; i++) {
    if (this->fields & (RAMSES_F_ADDR0 << i)) {
      sum += this->addr[i][0] + this->addr[i][1] + this->addr[i][2];
    }
  }
  if (this->fields & RAMSES_F_PARAM0)
    sum += this->param[0];
  if (this->fields & RAMSES_F_PARAM1)
    sum += this->param[1];
  sum += this->opcode[0] + this->opcode[1];
  sum += this->len;
  for (uint8_t i = 0; i < this->len; i++) {
    sum += this->payload[i];
  }
  sum += this->csum;
  return sum == 0;
}

std::string RamsesMessage::to_hgi80() const {
  char buf[256];
  int pos = 0;

  // RSSI / marker: "--- " or "%03u "
  if (this->rssi > 0) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%03u ", this->rssi);
  } else {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "--- ");
  }

  // Type: "RQ ", " I ", " W ", "RP "
  uint8_t type_idx = static_cast<uint8_t>(this->type);
  if (type_idx < RAMSES_MSG_MAX) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%2s ",
                    MSG_TYPE_NAMES[type_idx]);
  } else {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "?? ");
  }

  // Param0
  if (this->fields & RAMSES_F_PARAM0) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%03u ", this->param[0]);
  } else {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "--- ");
  }

  // Addr0, Addr1, Addr2
  for (uint8_t i = 0; i < RAMSES_MAX_ADDR; i++) {
    if (this->fields & (RAMSES_F_ADDR0 << i)) {
      RamsesAddress addr = RamsesAddress::from_bytes(this->addr[i]);
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%s ",
                      addr.to_string().c_str());
    } else {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "--:------ ");
    }
  }

  // Opcode
  pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X%02X ", this->opcode[0],
                  this->opcode[1]);

  // Payload Length
  pos += snprintf(buf + pos, sizeof(buf) - pos, "%03u", this->len);

  // Payload Bytes
  if (this->len > 0) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, " ");
    for (uint8_t i = 0; i < this->len && pos < (int)sizeof(buf) - 3; i++) {
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X", this->payload[i]);
    }
  }

  return std::string(buf);
}

bool RamsesMessage::from_hgi80(const std::string &line) {
  this->reset();
  std::istringstream iss(line);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }

  if (tokens.size() < 5) {
    ESP_LOGW(TAG, "HGI80 line too short (%d tokens)", (int)tokens.size());
    return false;
  }

  // Find the verb index (RQ, I, W, RP)
  int verb_idx = -1;
  for (size_t i = 0; i < tokens.size() && i < 3; i++) {
    if (tokens[i] == "RQ" || tokens[i] == "I" || tokens[i] == "W" ||
        tokens[i] == "RP") {
      verb_idx = static_cast<int>(i);
      break;
    }
  }

  if (verb_idx == -1)
    return false;

  // If there is an RSSI token before the verb
  if (verb_idx > 0) {
    if (tokens[0] != "---" && tokens[0] != "..." && tokens[0] != "???") {
      this->rssi = std::strtoul(tokens[0].c_str(), nullptr, 10);
    }
  }

  // Verb: RQ, I, W, RP
  std::string verb = tokens[verb_idx];
  if (verb == "RQ")
    this->type = RAMSES_MSG_RQ;
  else if (verb == "I")
    this->type = RAMSES_MSG_I;
  else if (verb == "W")
    this->type = RAMSES_MSG_W;
  else if (verb == "RP")
    this->type = RAMSES_MSG_RP;
  else
    return false;

  size_t idx = verb_idx + 1;

  // Optional sequence / Param0 (if not an address e.g. doesn't have ':' and
  // isn't '--:------')
  if (idx < tokens.size() && tokens[idx].find(':') == std::string::npos &&
      tokens[idx] != "--:------") {
    if (tokens[idx] != "---" && tokens[idx] != "..." && tokens[idx] != "???") {
      this->param[0] = std::strtoul(tokens[idx].c_str(), nullptr, 10);
      this->fields |= RAMSES_F_PARAM0;
    }
    idx++;
  }

  // Addresses (up to 3)
  for (uint8_t i = 0; i < RAMSES_MAX_ADDR && idx < tokens.size(); i++) {
    if (tokens[idx].find(':') != std::string::npos) {
      RamsesAddress addr = RamsesAddress::from_string(tokens[idx]);
      if (addr.is_valid) {
        addr.to_bytes(this->addr[i]);
        this->fields |= (RAMSES_F_ADDR0 << i);
      }
      idx++;
    } else if (tokens[idx] == "--:------") {
      idx++;
    } else {
      break;
    }
  }

  // Opcode (4 hex characters e.g. 1F09)
  if (idx < tokens.size() && tokens[idx].length() == 4) {
    unsigned int op = 0;
    if (sscanf(tokens[idx].c_str(), "%04x", &op) == 1) {
      this->opcode[0] = (op >> 8) & 0xFF;
      this->opcode[1] = op & 0xFF;
      this->rx_fields |= RAMSES_F_OPCODE;
      idx++;
    } else {
      return false;
    }
  } else {
    return false;
  }

  // Length (decimal or hex, typically 3 digits e.g. 003)
  if (idx < tokens.size()) {
    this->len =
        static_cast<uint8_t>(std::strtoul(tokens[idx].c_str(), nullptr, 10));
    this->rx_fields |= RAMSES_F_LEN;
    idx++;
  } else {
    return false;
  }

  // Payload (hex string)
  if (idx < tokens.size() && this->len > 0) {
    std::string payload_hex = tokens[idx];
    size_t expected_hex_len = static_cast<size_t>(this->len) * 2;
    if (payload_hex.length() >= expected_hex_len) {
      for (size_t i = 0; i < this->len && i < RAMSES_MAX_PAYLOAD; i++) {
        unsigned int b = 0;
        sscanf(payload_hex.substr(i * 2, 2).c_str(), "%02x", &b);
        this->payload[i] = static_cast<uint8_t>(b);
      }
      this->n_payload = this->len;
    }
  }

  this->csum = this->calculate_checksum();
  this->rx_fields |= this->fields;
  return true;
}

std::vector<uint8_t> RamsesMessage::to_raw_frame() const {
  std::vector<uint8_t> frame;
  // Preamble training sequence
  for (int i = 0; i < 20; i++) {
    frame.push_back(0x55);
  }
  // Sync bytes
  frame.push_back(0xFF);
  frame.push_back(0x00);
  frame.push_back(0x33);
  frame.push_back(0x55);
  frame.push_back(0x53);

  // Collect unencoded packet bytes
  std::vector<uint8_t> raw_bytes;
  raw_bytes.push_back(ramses_encode_header(this->fields));
  for (uint8_t i = 0; i < RAMSES_MAX_ADDR; i++) {
    if (this->fields & (RAMSES_F_ADDR0 << i)) {
      raw_bytes.push_back(this->addr[i][0]);
      raw_bytes.push_back(this->addr[i][1]);
      raw_bytes.push_back(this->addr[i][2]);
    }
  }
  if (this->fields & RAMSES_F_PARAM0)
    raw_bytes.push_back(this->param[0]);
  if (this->fields & RAMSES_F_PARAM1)
    raw_bytes.push_back(this->param[1]);
  raw_bytes.push_back(this->opcode[0]);
  raw_bytes.push_back(this->opcode[1]);
  raw_bytes.push_back(this->len);
  for (uint8_t i = 0; i < this->len; i++) {
    raw_bytes.push_back(this->payload[i]);
  }
  raw_bytes.push_back(this->calculate_checksum());

  // Manchester encode each byte (high nibble then low nibble)
  for (uint8_t b : raw_bytes) {
    frame.push_back(manchester_encode((b >> 4) & 0x0F));
    frame.push_back(manchester_encode(b & 0x0F));
  }

  // Trailer & trailing training
  frame.push_back(0x35);
  frame.push_back(0x55);
  return frame;
}

} // namespace ramses_esp
} // namespace esphome
