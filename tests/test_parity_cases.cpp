#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include <vector>
#include <cstring>
#include "components/ramses_esp/ramses_message.h"
#include "components/ramses_esp/ramses_decoder.h"

using namespace esphome::ramses_esp;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
  do { \
    tests_run++; \
    if (cond) { \
      tests_passed++; \
      std::cout << "  [PASS] " << msg << "\n"; \
    } else { \
      std::cerr << "  [FAIL] " << msg << " (Line " << __LINE__ << ")\n"; \
      assert(false); \
    } \
  } while (0)

// Helper to extract JSON string property by key
static std::string extract_json_str(const std::string &json, const std::string &key) {
  std::string pattern = "\"" + key + "\": \"";
  size_t start = json.find(pattern);
  if (start == std::string::npos) return "";
  start += pattern.length();
  size_t end = json.find("\"", start);
  if (end == std::string::npos) return "";
  return json.substr(start, end - start);
}

static int extract_json_int(const std::string &json, const std::string &key, int default_val = -1) {
  std::string pattern = "\"" + key + "\": ";
  size_t start = json.find(pattern);
  if (start == std::string::npos) return default_val;
  start += pattern.length();
  return std::strtol(json.c_str() + start, nullptr, 10);
}

static float extract_json_float(const std::string &json, const std::string &key, float default_val = -1.0f) {
  std::string pattern = "\"" + key + "\": ";
  size_t start = json.find(pattern);
  if (start == std::string::npos) return default_val;
  start += pattern.length();
  return std::strtof(json.c_str() + start, nullptr);
}

void run_parity_fixture(const std::string &fixture_path) {
  std::ifstream file(fixture_path);
  if (!file.is_open()) {
    std::cerr << "Could not open fixture file: " << fixture_path << "\n";
    assert(false);
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  // Split fixture by object blocks { ... }
  size_t pos = 0;
  while ((pos = content.find('{', pos)) != std::string::npos) {
    size_t end = content.find('}', pos);
    if (end == std::string::npos) break;
    std::string block = content.substr(pos, end - pos + 1);
    pos = end + 1;

    std::string name = extract_json_str(block, "name");
    std::string hgi80 = extract_json_str(block, "hgi80");
    std::string expected_verb = extract_json_str(block, "verb");
    std::string expected_src = extract_json_str(block, "src");
    std::string expected_dst = extract_json_str(block, "dst");
    std::string expected_opcode = extract_json_str(block, "opcode");

    if (hgi80.empty() || expected_opcode.empty()) continue;

    std::cout << "\nValidating Case: " << name << " (" << expected_opcode << ")\n";

    RamsesMessage msg;
    bool ok = msg.from_hgi80(hgi80);
    TEST_ASSERT(ok, "HGI80 parsing succeeded");

    // Check header fields
    char op_str[5];
    snprintf(op_str, sizeof(op_str), "%02X%02X", msg.opcode[0], msg.opcode[1]);
    TEST_ASSERT(std::string(op_str) == expected_opcode, "Opcode matches expected hex");

    RamsesAddress src = RamsesAddress::from_bytes(msg.addr[0]);
    TEST_ASSERT(src.to_string() == expected_src, "Source address matches expected");

    if (!expected_dst.empty() && expected_dst != "--:------") {
      RamsesAddress dst1 = RamsesAddress::from_bytes(msg.addr[1]);
      RamsesAddress dst2 = RamsesAddress::from_bytes(msg.addr[2]);
      bool dst_matches = (dst1.to_string() == expected_dst) || (dst2.to_string() == expected_dst);
      TEST_ASSERT(dst_matches, "Destination/Target address matches expected");
    }

    // Specific opcode payload semantic tests
    if (expected_opcode == "0004") {
      std::string exp_name = extract_json_str(block, "zone_name");
      if (!exp_name.empty()) {
        std::string actual_name(reinterpret_cast<char*>(&msg.payload[2]));
        TEST_ASSERT(actual_name.rfind(exp_name, 0) == 0, "Decoded zone name matches expected string");
      }
    } else if (expected_opcode == "2309") {
      float exp_sp = extract_json_float(block, "setpoint");
      if (exp_sp > 0) {
        uint16_t raw_sp = ((uint16_t)msg.payload[1] << 8) | msg.payload[2];
        float actual_sp = raw_sp / 100.0f;
        TEST_ASSERT(std::abs(actual_sp - exp_sp) < 0.05f, "Decoded setpoint matches expected float");
      }
    } else if (expected_opcode == "3150") {
      float exp_dem = extract_json_float(block, "demand_pct");
      if (exp_dem >= 0) {
        float actual_dem = (msg.payload[1] / 200.0f) * 100.0f;
        TEST_ASSERT(std::abs(actual_dem - exp_dem) < 0.1f, "Decoded heat demand % matches expected");
      }
    } else if (expected_opcode == "1060") {
      int exp_bat = extract_json_int(block, "battery_pct");
      if (exp_bat >= 0) {
        int actual_bat = msg.payload[1];
        TEST_ASSERT(actual_bat == exp_bat, "Decoded battery % matches expected");
      }
    } else if (expected_opcode == "10E0") {
      std::string exp_oem = extract_json_str(block, "oem_code");
      if (exp_oem == "0x67") {
        TEST_ASSERT(msg.payload[6] == 0x67, "Decoded OEM signature matches 0x67 (Orcon)");
      }
    } else if (expected_opcode == "10D0") {
      int exp_days = extract_json_int(block, "filter_remaining_days");
      if (exp_days >= 0) {
        uint16_t actual_days = msg.payload[1];
        TEST_ASSERT(actual_days == (uint16_t)exp_days, "Decoded filter days match expected");
      }
    } else if (expected_opcode == "12C0") {
      auto dec = OutdoorTemperaturePayload::decode(msg.payload, msg.n_payload);
      float exp_temp = extract_json_float(block, "temperature");
      TEST_ASSERT(dec.has_value() && dec->is_valid, "Decoded outdoor temp payload");
      TEST_ASSERT(std::abs(dec->temperature - exp_temp) < 0.05f, "Outdoor temp matches expected");
    } else if (expected_opcode == "1260") {
      auto dec = DhwStatePayload::decode_temp(msg.payload, msg.n_payload);
      float exp_temp = extract_json_float(block, "temperature");
      TEST_ASSERT(dec.has_value(), "Decoded DHW temp payload");
      TEST_ASSERT(std::abs(dec->current_temp - exp_temp) < 0.05f, "DHW temp matches expected");
    } else if (expected_opcode == "12F0") {
      auto dec = DhwConfigPayload::decode(msg.payload, msg.n_payload);
      float exp_sp = extract_json_float(block, "setpoint");
      TEST_ASSERT(dec.has_value(), "Decoded DHW config payload");
      TEST_ASSERT(std::abs(dec->setpoint_temperature - exp_sp) < 0.05f, "DHW setpoint matches expected");
    } else if (expected_opcode == "0008") {
      auto dec = RelayDemandPayload::decode(msg.payload, msg.n_payload);
      float exp_dem = extract_json_float(block, "demand_pct");
      TEST_ASSERT(dec.has_value(), "Decoded relay demand payload");
      TEST_ASSERT(std::abs(dec->demand_percent - exp_dem) < 0.1f, "Relay demand matches expected");
    } else if (expected_opcode == "1298") {
      auto dec = Co2SensorPayload::decode(msg.payload, msg.n_payload);
      int exp_co2 = extract_json_int(block, "co2_ppm");
      TEST_ASSERT(dec.has_value() && dec->is_valid, "Decoded CO2 payload");
      TEST_ASSERT(dec->co2_ppm == exp_co2, "CO2 ppm matches expected");
    }
  }
}

int main(int argc, char **argv) {
  std::string fixture_path = "fixtures/parity_cases.json";
  if (argc > 1) {
    fixture_path = argv[1];
  } else {
    // Try relative to binary or repo root
    std::ifstream test(fixture_path);
    if (!test.is_open()) {
      fixture_path = "../fixtures/parity_cases.json";
    }
  }

  std::cout << "========================================\n";
  std::cout << "Running C++ Parity Test against JSON Fixture: " << fixture_path << "\n";
  std::cout << "========================================\n";

  run_parity_fixture(fixture_path);

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run && tests_run > 0) ? 0 : 1;
}
