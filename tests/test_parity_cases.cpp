#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace esphome::ramses_esp;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    tests_run++;                                                               \
    if (cond) {                                                                \
      tests_passed++;                                                          \
      std::cout << "  [PASS] " << msg << "\n";                                 \
    } else {                                                                   \
      std::cerr << "  [FAIL] " << msg << " (Line " << __LINE__ << ")\n";       \
      assert(false);                                                           \
    }                                                                          \
  } while (0)

// The parity fixture is intentionally limited to an array of objects with a
// nested "expected" object. These helpers validate that contract without adding
// a runtime JSON dependency to the native test binary.
static bool has_json_key(const std::string &json, const std::string &key) {
  return json.find("\"" + key + "\"") != std::string::npos;
}

static std::string extract_json_str(const std::string &json,
                                    const std::string &key) {
  size_t start = json.find("\"" + key + "\"");
  if (start == std::string::npos)
    return "";
  start = json.find(':', start);
  if (start == std::string::npos)
    return "";
  start = json.find_first_not_of(" \t\r\n", start + 1);
  if (start == std::string::npos || json[start] != '"')
    return "";
  std::string value;
  bool escaped = false;
  for (size_t index = start + 1; index < json.size(); index++) {
    char character = json[index];
    if (escaped) {
      value += character;
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return value;
    } else {
      value += character;
    }
  }
  return "";
}

static int extract_json_int(const std::string &json, const std::string &key,
                            int default_val = -1) {
  size_t start = json.find("\"" + key + "\"");
  if (start == std::string::npos)
    return default_val;
  start = json.find(':', start);
  if (start == std::string::npos)
    return default_val;
  start = json.find_first_not_of(" \t\r\n", start + 1);
  if (start == std::string::npos)
    return default_val;
  return std::strtol(json.c_str() + start, nullptr, 10);
}

static float extract_json_float(const std::string &json, const std::string &key,
                                float default_val = -1.0f) {
  size_t start = json.find("\"" + key + "\"");
  if (start == std::string::npos)
    return default_val;
  start = json.find(':', start);
  if (start == std::string::npos)
    return default_val;
  start = json.find_first_not_of(" \t\r\n", start + 1);
  if (start == std::string::npos)
    return default_val;
  return std::strtof(json.c_str() + start, nullptr);
}

static bool extract_json_bool(const std::string &json, const std::string &key,
                              bool default_val = false) {
  size_t start = json.find("\"" + key + "\"");
  if (start == std::string::npos)
    return default_val;
  start = json.find(':', start);
  if (start == std::string::npos)
    return default_val;
  start = json.find_first_not_of(" \t\r\n", start + 1);
  if (start == std::string::npos)
    return default_val;
  return json.compare(start, 4, "true") == 0;
}

static std::vector<float> extract_json_floats(const std::string &json,
                                              const std::string &key) {
  std::vector<float> values;
  size_t search_start = 0;
  while (true) {
    size_t key_start = json.find("\"" + key + "\"", search_start);
    if (key_start == std::string::npos)
      return values;
    size_t colon = json.find(':', key_start);
    if (colon == std::string::npos)
      return values;
    size_t value_start = json.find_first_not_of(" \t\r\n", colon + 1);
    if (value_start == std::string::npos)
      return values;
    values.push_back(std::strtof(json.c_str() + value_start, nullptr));
    search_start = value_start + 1;
  }
}

static size_t find_json_object_end(const std::string &content, size_t start) {
  int depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (size_t index = start; index < content.size(); index++) {
    char character = content[index];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == '"') {
        in_string = false;
      }
      continue;
    }
    if (character == '"') {
      in_string = true;
    } else if (character == '{') {
      depth++;
    } else if (character == '}' && --depth == 0) {
      return index;
    }
  }
  return std::string::npos;
}

void run_parity_fixture(const std::string &fixture_path) {
  std::ifstream file(fixture_path);
  if (!file.is_open()) {
    std::cerr << "Could not open fixture file: " << fixture_path << "\n";
    assert(false);
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  size_t first_non_whitespace = content.find_first_not_of(" \t\r\n");
  size_t last_non_whitespace = content.find_last_not_of(" \t\r\n");
  TEST_ASSERT(first_non_whitespace != std::string::npos &&
                  content[first_non_whitespace] == '[',
              "Parity fixture is a JSON array");
  TEST_ASSERT(last_non_whitespace != std::string::npos &&
                  content[last_non_whitespace] == ']',
              "Parity fixture has a closing JSON array");

  // Extract complete outer objects, including their nested "expected" object.
  size_t pos = 0;
  while ((pos = content.find('{', pos)) != std::string::npos) {
    size_t end = find_json_object_end(content, pos);
    if (end == std::string::npos)
      break;
    std::string block = content.substr(pos, end - pos + 1);
    pos = end + 1;

    std::string name = extract_json_str(block, "name");
    std::string hgi80 = extract_json_str(block, "hgi80");
    std::string expected_verb = extract_json_str(block, "verb");
    std::string expected_src = extract_json_str(block, "src");
    std::string expected_dst = extract_json_str(block, "dst");
    std::string expected_opcode = extract_json_str(block, "opcode");

    TEST_ASSERT(has_json_key(block, "name") && has_json_key(block, "hgi80") &&
                    has_json_key(block, "expected"),
                "Parity case has required fields");
    TEST_ASSERT(!hgi80.empty() && !expected_opcode.empty() &&
                    !expected_verb.empty() && !expected_src.empty(),
                "Parity case required values are valid");

    std::cout << "\nValidating Case: " << name << " (" << expected_opcode
              << ")\n";

    RamsesMessage msg;
    bool ok = msg.from_hgi80(hgi80);
    TEST_ASSERT(ok, "HGI80 parsing succeeded");

    // Check header fields
    char op_str[5];
    snprintf(op_str, sizeof(op_str), "%02X%02X", msg.opcode[0], msg.opcode[1]);
    TEST_ASSERT(std::string(op_str) == expected_opcode,
                "Opcode matches expected hex");

    RamsesAddress src = RamsesAddress::from_bytes(msg.addr[0]);
    TEST_ASSERT(src.to_string() == expected_src,
                "Source address matches expected");

    if (!expected_dst.empty() && expected_dst != "--:------") {
      RamsesAddress dst1 = RamsesAddress::from_bytes(msg.addr[1]);
      RamsesAddress dst2 = RamsesAddress::from_bytes(msg.addr[2]);
      bool dst_matches = (dst1.to_string() == expected_dst) ||
                         (dst2.to_string() == expected_dst);
      TEST_ASSERT(dst_matches, "Destination/Target address matches expected");
    }

    // Specific opcode payload semantic tests
    if (expected_opcode == "1F09") {
      auto dec = SystemSyncPayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value(), "Decoded system sync payload");
      TEST_ASSERT(has_json_key(block, "system_mode_raw") &&
                      dec->mode_raw ==
                          extract_json_int(block, "system_mode_raw"),
                  "System mode raw value matches expected");
      TEST_ASSERT(has_json_key(block, "remaining_raw") &&
                      dec->remaining_raw ==
                          extract_json_int(block, "remaining_raw"),
                  "System sync remaining value matches expected");
      TEST_ASSERT(has_json_key(block, "system_mode") &&
                      system_mode_to_string(dec->mode) ==
                          extract_json_str(block, "system_mode"),
                  "System mode matches expected");
    } else if (expected_opcode == "2E04") {
      auto dec = SystemModePayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value(), "Decoded system mode payload");
      TEST_ASSERT(has_json_key(block, "system_mode_raw") &&
                      dec->mode_raw ==
                          extract_json_int(block, "system_mode_raw"),
                  "System mode raw value matches expected");
      TEST_ASSERT(has_json_key(block, "system_mode") &&
                      system_mode_to_string(dec->mode) ==
                          extract_json_str(block, "system_mode"),
                  "System mode matches expected");
    } else if (expected_opcode == "0004") {
      std::string exp_name = extract_json_str(block, "zone_name");
      auto dec = ZoneNamePayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value(), "Decoded zone name payload");
      TEST_ASSERT(has_json_key(block, "zone_index") &&
                      dec->zone_index == extract_json_int(block, "zone_index"),
                  "Zone name index matches expected");
      TEST_ASSERT(dec->name == exp_name,
                  "Decoded zone name matches expected string");
    } else if (expected_opcode == "2309") {
      auto dec = SetpointPayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value() && dec->zones.size() == 1,
                  "Decoded setpoint payload");
      TEST_ASSERT(has_json_key(block, "zone_index") &&
                      dec->zones[0].zone_index ==
                          extract_json_int(block, "zone_index"),
                  "Setpoint zone index matches expected");
      TEST_ASSERT(std::abs(dec->zones[0].setpoint -
                           extract_json_float(block, "setpoint")) < 0.05f,
                  "Decoded setpoint matches expected float");
    } else if (expected_opcode == "30C9") {
      auto dec = TemperaturePayload::decode(msg.payload, msg.n_payload);
      auto expected_temperatures = extract_json_floats(block, "temperature");
      TEST_ASSERT(dec.has_value() &&
                      dec->zones.size() == expected_temperatures.size(),
                  "Decoded all expected zone temperatures");
      for (size_t index = 0; index < expected_temperatures.size(); index++) {
        TEST_ASSERT(std::abs(dec->zones[index].temperature -
                             expected_temperatures[index]) < 0.05f,
                    "Decoded zone temperature matches expected");
      }
    } else if (expected_opcode == "3150") {
      auto dec = HeatDemandPayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value(), "Decoded heat demand payload");
      TEST_ASSERT(dec->domain_or_zone_index ==
                      extract_json_int(block, "domain_or_zone_index"),
                  "Heat demand zone index matches expected");
      TEST_ASSERT(std::abs(dec->demand_percent -
                           extract_json_float(block, "demand_pct")) < 0.1f,
                  "Decoded heat demand % matches expected");
    } else if (expected_opcode == "1060") {
      auto dec = DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value(), "Decoded battery payload");
      TEST_ASSERT(dec->battery_percent ==
                      extract_json_int(block, "battery_pct"),
                  "Decoded battery % matches expected");
      TEST_ASSERT(dec->battery_low == extract_json_bool(block, "battery_low"),
                  "Decoded battery state matches expected");
    } else if (expected_opcode == "10E0") {
      auto dec = DeviceInfoPayload::decode(msg.payload, msg.n_payload);
      int expected_oem =
          std::stoi(extract_json_str(block, "oem_code"), nullptr, 16);
      TEST_ASSERT(dec.has_value() && dec->oem_code == expected_oem,
                  "Decoded OEM signature matches expected");
    } else if (expected_opcode == "10D0") {
      auto dec = FilterInfoPayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value() &&
                      dec->remaining_days ==
                          extract_json_int(block, "filter_remaining_days"),
                  "Decoded filter days match expected");
    } else if (expected_opcode == "12C0") {
      auto dec = OutdoorTemperaturePayload::decode(msg.payload, msg.n_payload);
      float exp_temp = extract_json_float(block, "temperature");
      TEST_ASSERT(dec.has_value() && dec->is_valid,
                  "Decoded outdoor temp payload");
      TEST_ASSERT(std::abs(dec->temperature - exp_temp) < 0.05f,
                  "Outdoor temp matches expected");
    } else if (expected_opcode == "1260") {
      auto dec = DhwStatePayload::decode_temp(msg.payload, msg.n_payload);
      float exp_temp = extract_json_float(block, "temperature");
      TEST_ASSERT(dec.has_value(), "Decoded DHW temp payload");
      TEST_ASSERT(std::abs(dec->current_temp - exp_temp) < 0.05f,
                  "DHW temp matches expected");
    } else if (expected_opcode == "12F0") {
      auto dec = DhwConfigPayload::decode(msg.payload, msg.n_payload);
      float exp_flow_rate = extract_json_float(block, "flow_rate");
      TEST_ASSERT(dec.has_value(), "Decoded DHW flow-rate payload");
      TEST_ASSERT(std::abs(dec->flow_rate - exp_flow_rate) < 0.05f,
                  "DHW flow rate matches expected");
    } else if (expected_opcode == "0008") {
      auto dec = RelayDemandPayload::decode(msg.payload, msg.n_payload);
      float exp_dem = extract_json_float(block, "demand_pct");
      TEST_ASSERT(dec.has_value(), "Decoded relay demand payload");
      TEST_ASSERT(std::abs(dec->demand_percent - exp_dem) < 0.1f,
                  "Relay demand matches expected");
    } else if (expected_opcode == "22F1") {
      auto dec = FanStatePayload::decode(msg.payload, msg.n_payload,
                                         HvacScheme::ORCON);
      TEST_ASSERT(dec.has_value(), "Decoded fan state payload");
      TEST_ASSERT(dec->raw_mode == extract_json_int(block, "fan_mode_raw"),
                  "Fan raw mode matches expected");
      TEST_ASSERT(fan_preset_to_string(dec->preset_mode) ==
                      extract_json_str(block, "fan_mode_name"),
                  "Fan preset mode matches expected");
    } else if (expected_opcode == "3220") {
      auto dec = OpenThermPayload::decode(msg.payload, msg.n_payload);
      TEST_ASSERT(dec.has_value(), "Decoded OpenTherm payload");
      TEST_ASSERT(dec->flame_active == extract_json_bool(block, "flame_active"),
                  "Flame state matches expected");
      TEST_ASSERT(std::abs(dec->modulation_percent -
                           extract_json_float(block, "modulation_pct")) < 0.1f,
                  "OpenTherm modulation matches expected");
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
  std::cout << "Running C++ Parity Test against JSON Fixture: " << fixture_path
            << "\n";
  std::cout << "========================================\n";

  run_parity_fixture(fixture_path);

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run
            << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run && tests_run > 0) ? 0 : 1;
}
