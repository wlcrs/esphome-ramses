#include "components/ramses_discovery/ramses_discovery.h"
#include "components/ramses_esp/ramses_decoder.h"
#include "esphome/core/application.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace esphome;
using namespace esphome::ramses_esp;
using namespace esphome::ramses_discovery;

Application esphome::App;

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

static std::string extract_hgi80_frame(const std::string &line) {
  size_t hash_pos = line.find('#');
  std::string cleaned =
      (hash_pos != std::string::npos) ? line.substr(0, hash_pos) : line;

  size_t start = cleaned.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = cleaned.find_last_not_of(" \t\r\n");
  cleaned = cleaned.substr(start, end - start + 1);

  if (cleaned.empty())
    return "";

  if (cleaned.size() > 27 && (cleaned[4] == '-' || cleaned[10] == 'T')) {
    size_t space_pos = cleaned.find(' ');
    if (space_pos != std::string::npos) {
      cleaned = cleaned.substr(space_pos + 1);
      size_t s2 = cleaned.find_first_not_of(" \t\r\n");
      if (s2 != std::string::npos) {
        cleaned = cleaned.substr(s2);
      }
    }
  }
  return cleaned;
}

static RamsesMessage parse_msg(const std::string &hgi80) {
  RamsesMessage msg;
  msg.from_hgi80(hgi80);
  return msg;
}

static std::string find_corpus_file(const std::string &rel_path) {
  std::vector<std::string> prefixes = {
      "fixtures/corpus/", "../fixtures/corpus/", "tests/fixtures/corpus/",
      "../../tests/fixtures/corpus/",
      "../ramses_rf/tests/tests_rf/data_driven/"};
  for (const auto &p : prefixes) {
    std::string full = p + rel_path;
    if (fs::exists(full))
      return full;
  }
  return "";
}

void test_passive_discovery() {
  std::cout << "\n--- Testing RamsesDiscovery Synthetic Ingestion ---\n";
  RamsesDiscoveryComponent discovery;

  // 1. Controller packets
  discovery.on_message(parse_msg(
      "045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801073A"));
  discovery.on_message(
      parse_msg("045  I --- 01:145678 --:------ 01:145678 2309 003 00079E"));
  discovery.on_message(
      parse_msg("045 RP --- 01:145678 18:005612 --:------ 0004 022 "
                "00004C6F756E67650000000000000000000000000000"));
  discovery.on_message(
      parse_msg("045  I --- 01:145678 --:------ 01:145678 1260 003 000837"));

  // 2. HVAC Fan packets
  discovery.on_message(
      parse_msg("045  I --- 32:155617 --:------ 32:155617 22F1 003 0002FF"));
  discovery.on_message(
      parse_msg("045 RP --- 32:155617 18:005612 --:------ 10E0 038 "
                "00000000000067000000000000000000000000000000000000000000000000"
                "00000000000000"));
  discovery.on_message(parse_msg(
      "045 RP --- 32:155617 18:005612 --:------ 10D0 006 00B4B4C80000"));

  // 3. TRV packets
  discovery.on_message(
      parse_msg("045  I --- 04:089123 --:------ 01:145678 3150 002 0046"));
  discovery.on_message(
      parse_msg("045  I --- 04:089123 --:------ 04:089123 1060 003 005801"));

  // 4. OpenTherm packets
  discovery.on_message(parse_msg(
      "045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000"));

  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.size() == 4, "Discovered 4 distinct devices");

  // Verify Controller
  TEST_ASSERT(devices.count("01:145678") == 1,
              "Controller 01:145678 discovered");
  const auto &ctl = devices.at("01:145678");
  TEST_ASSERT(ctl.device_type == "controller", "Device type is controller");
  TEST_ASSERT(ctl.zones.size() == 2, "Controller has 2 zones");
  TEST_ASSERT(ctl.zones.at(0).name == "Lounge", "Zone 0 name is 'Lounge'");
  TEST_ASSERT(ctl.has_dhw == true, "Controller has DHW enabled");

  // Verify HVAC Unit
  TEST_ASSERT(devices.count("32:155617") == 1,
              "HVAC Unit 32:155617 discovered");
  const auto &hvac = devices.at("32:155617");
  TEST_ASSERT(hvac.is_hvac == true, "HVAC flag is true");
  TEST_ASSERT(hvac.oem_name == "orcon", "OEM Scheme identified as 'orcon'");

  // Verify TRV
  TEST_ASSERT(devices.count("04:089123") == 1, "TRV 04:089123 discovered");
  const auto &trv = devices.at("04:089123");
  TEST_ASSERT(trv.device_type == "trv", "Device type is trv");

  // Verify OpenTherm
  TEST_ASSERT(devices.count("10:045678") == 1,
              "OpenTherm 10:045678 discovered");
  const auto &ot = devices.at("10:045678");
  TEST_ASSERT(ot.device_type == "opentherm", "Device type is opentherm");
}

void test_yaml_generation() {
  std::cout << "\n--- Testing Auto-Generated YAML Output ---\n";
  RamsesDiscoveryComponent discovery;

  discovery.on_message(parse_msg(
      "045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801073A"));
  discovery.on_message(
      parse_msg("045 RP --- 01:145678 18:005612 --:------ 0004 022 "
                "00004C6F756E67650000000000000000000000000000"));
  discovery.on_message(
      parse_msg("045  I --- 01:145678 --:------ 01:145678 1260 003 000837"));
  discovery.on_message(
      parse_msg("045  I --- 32:155617 --:------ 32:155617 22F1 003 0002FF"));
  discovery.on_message(
      parse_msg("045 RP --- 32:155617 18:005612 --:------ 10E0 038 "
                "00000000000067000000000000000000000000000000000000000000000000"
                "00000000000000"));
  discovery.on_message(
      parse_msg("045  I --- 04:089123 --:------ 01:145678 3150 002 0046"));
  discovery.on_message(parse_msg(
      "045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000"));

  std::string yaml = discovery.generate_yaml();

  TEST_ASSERT(yaml.find("devices:") != std::string::npos,
              "YAML contains esphome devices list");
  TEST_ASSERT(yaml.find("- id: ramses_01_145678") != std::string::npos,
              "YAML contains controller subdevice declaration");
  TEST_ASSERT(yaml.find("name: \"Evohome Controller 01:145678\"") !=
                  std::string::npos,
              "YAML contains controller subdevice name");
  TEST_ASSERT(yaml.find("- id: ramses_32_155617") != std::string::npos,
              "YAML contains HVAC subdevice declaration");
  TEST_ASSERT(yaml.find("climate:") != std::string::npos,
              "YAML contains climate platform");
  TEST_ASSERT(yaml.find("name: \"Lounge Heating\"") != std::string::npos,
              "YAML contains 'Lounge Heating'");
  TEST_ASSERT(yaml.find("controller_address: \"01:145678\"") !=
                  std::string::npos,
              "YAML contains controller address");
  TEST_ASSERT(yaml.find("device_id: ramses_01_145678") != std::string::npos,
              "YAML contains controller device_id");
  TEST_ASSERT(yaml.find("water_heater:") != std::string::npos,
              "YAML contains water_heater platform");
  TEST_ASSERT(yaml.find("fan:") != std::string::npos,
              "YAML contains fan platform");
  TEST_ASSERT(yaml.find("device_id: ramses_32_155617") != std::string::npos,
              "YAML contains fan device_id");
  TEST_ASSERT(yaml.find("scheme: orcon") != std::string::npos,
              "YAML contains scheme: orcon");
  TEST_ASSERT(yaml.find("sensor:") != std::string::npos,
              "YAML contains sensor platform");
  TEST_ASSERT(yaml.find("device_id: ramses_04_089123") != std::string::npos,
              "YAML contains TRV device_id");
  TEST_ASSERT(yaml.find("device_id: ramses_10_045678") != std::string::npos,
              "YAML contains OpenTherm device_id");
  TEST_ASSERT(yaml.find("binary_sensor:") != std::string::npos,
              "YAML contains binary_sensor platform");
}

void test_real_world_system_logs() {
  std::cout
      << "\n--- Testing Discovery Against Real-World Evohome System Log ---\n";
  std::string log_file = find_corpus_file("systems/heat_zxdavb/packet.log");
  if (log_file.empty()) {
    std::cout << "  [SKIP] heat_zxdavb/packet.log not found\n";
    return;
  }

  std::ifstream infile(log_file);
  TEST_ASSERT(infile.is_open(), "Opened heat_zxdavb/packet.log");

  RamsesDiscoveryComponent discovery;
  std::string line;
  int packet_count = 0;
  while (std::getline(infile, line)) {
    std::string frame = extract_hgi80_frame(line);
    if (frame.empty())
      continue;
    RamsesMessage msg;
    if (msg.from_hgi80(frame)) {
      discovery.on_message(msg);
      packet_count++;
    }
  }

  std::cout << "  Ingested " << packet_count
            << " packets from real system log.\n";
  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.size() >= 5, "Discovered multiple real-world devices");
  TEST_ASSERT(devices.count("01:145038") == 1,
              "Discovered controller 01:145038");

  const auto &ctl = devices.at("01:145038");
  TEST_ASSERT(ctl.device_type == "controller",
              "Identified as Evohome controller");

  // Verify TRVs in the installation
  bool has_trv = false;
  for (const auto &kv : devices) {
    if (kv.second.device_type == "trv") {
      has_trv = true;
      break;
    }
  }
  TEST_ASSERT(has_trv, "Discovered TRV radiator valves in topology");

  std::string yaml = discovery.generate_yaml();
  TEST_ASSERT(
      yaml.find("climate:") != std::string::npos ||
          yaml.find("sensor:") != std::string::npos,
      "Generated valid ESPHome configuration from real-world packet stream");
}

void test_real_world_opentherm_log() {
  std::cout << "\n--- Testing Discovery Against Real-World OpenTherm System "
               "Log ---\n";
  std::string log_file = find_corpus_file("systems/heat_otb_00/packet.log");
  if (log_file.empty()) {
    std::cout << "  [SKIP] heat_otb_00/packet.log not found\n";
    return;
  }

  std::ifstream infile(log_file);
  TEST_ASSERT(infile.is_open(), "Opened heat_otb_00/packet.log");

  RamsesDiscoveryComponent discovery;
  std::string line;
  while (std::getline(infile, line)) {
    std::string frame = extract_hgi80_frame(line);
    if (frame.empty())
      continue;
    RamsesMessage msg;
    if (msg.from_hgi80(frame)) {
      discovery.on_message(msg);
    }
  }

  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.count("10:048122") == 1,
              "Discovered OpenTherm Bridge 10:048122");
  const auto &ot = devices.at("10:048122");
  TEST_ASSERT(ot.device_type == "opentherm", "Identified as OpenTherm Bridge");
}

void test_hopper_d375_discovery() {
  std::cout << "\n--- Testing Hopper D375 HRU Auto-Discovery ---\n";
  RamsesDiscoveryComponent discovery;

  // Hopper D375 VMD-02RPS54 fingerprint and fan status
  discovery.on_message(
      parse_msg("068  I --- 32:137527 63:262142 --:------ 10E0 038 "
                "000001C84F0E0A6AFEFFFFFFFFFF0B0C07E1564D442D303252505335340000"
                "00000000000000"));
  discovery.on_message(
      parse_msg("072  I --- 32:137527 --:------ 32:137527 22F1 003 0002FF"));

  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.count("32:137527") == 1,
              "Discovered Hopper D375 HRU 32:137527");
  const auto &hvac = devices.at("32:137527");
  TEST_ASSERT(hvac.is_hvac == true, "Identified as HVAC unit");
  TEST_ASSERT(hvac.oem_name == "hopper",
              "Hopper D375 mapped to 'hopper' scheme");

  std::string yaml = discovery.generate_yaml();
  TEST_ASSERT(yaml.find("devices:") != std::string::npos,
              "YAML contains esphome devices list");
  TEST_ASSERT(yaml.find("- id: ramses_32_137527") != std::string::npos,
              "YAML contains Hopper D375 subdevice declaration");
  TEST_ASSERT(yaml.find("fan:") != std::string::npos,
              "YAML contains fan platform");
  TEST_ASSERT(yaml.find("device_address: \"32:137527\"") != std::string::npos,
              "YAML contains Hopper D375 address");
  TEST_ASSERT(yaml.find("device_id: ramses_32_137527") != std::string::npos,
              "YAML contains Hopper D375 device_id");
  TEST_ASSERT(yaml.find("scheme: hopper") != std::string::npos,
              "YAML specifies scheme: hopper");
}

void test_json_generation_and_device_yaml() {
  std::cout
      << "\n--- Testing Discovery JSON & Per-Device YAML Generation ---\n";
  RamsesDiscoveryComponent discovery;

  // 1. Controller with zone
  discovery.on_message(
      parse_msg("064  I --- 01:145678 --:------ 01:145678 0005 004 00080000"));
  discovery.on_message(
      parse_msg("064  I --- 01:145678 --:------ 01:145678 0004 022 "
                "00004C6976696E6720526F6F6D000000000000000000"));
  discovery.on_message(
      parse_msg("064  I --- 01:145678 --:------ 01:145678 30C9 003 000850"));

  // 2. MVHR Unit
  discovery.on_message(
      parse_msg("072  I --- 32:137527 63:262142 --:------ 10E0 038 "
                "000001C84F0E0A6AFEFFFFFFFFFF0B0C07E1564D442D303252505335340000"
                "00000000000000"));

  // 3. TRV
  discovery.on_message(
      parse_msg("080  I --- 04:089123 --:------ 01:145678 3150 002 0064"));

  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.size() == 3, "Discovered 3 devices total");

  const auto &ctl = devices.at("01:145678");
  std::string ctl_yaml = discovery.generate_device_yaml(ctl);
  TEST_ASSERT(ctl_yaml.find("climate:") != std::string::npos,
              "Controller YAML contains climate platform");
  TEST_ASSERT(ctl_yaml.find("Living Room") != std::string::npos,
              "Controller YAML contains Living Room");
  TEST_ASSERT(ctl_yaml.find("device_id: ramses_01_145678") != std::string::npos,
              "Controller YAML contains device_id");
  TEST_ASSERT(ctl_yaml.find("fan:") == std::string::npos,
              "Controller YAML does NOT contain fan platform");

  const auto &hvac = devices.at("32:137527");
  std::string hvac_yaml = discovery.generate_device_yaml(hvac);
  TEST_ASSERT(hvac_yaml.find("fan:") != std::string::npos,
              "HVAC YAML contains fan platform");
  TEST_ASSERT(hvac_yaml.find("device_id: ramses_32_137527") !=
                  std::string::npos,
              "HVAC YAML contains device_id");
  TEST_ASSERT(hvac_yaml.find("scheme: hopper") != std::string::npos,
              "HVAC YAML specifies scheme: hopper");

  std::string json = discovery.generate_json(2000);
  TEST_ASSERT(!json.empty(), "JSON output is not empty");
  TEST_ASSERT(json.find("\"device_count\": 3") != std::string::npos,
              "JSON contains device_count: 3");
  TEST_ASSERT(json.find("\"address\": \"01:145678\"") != std::string::npos,
              "JSON contains controller address");
  TEST_ASSERT(json.find("\"type_label\": \"Evohome Controller\"") !=
                  std::string::npos,
              "JSON contains controller type label");
  TEST_ASSERT(json.find("\"name\": \"Living Room\"") != std::string::npos,
              "JSON contains Living Room zone name");
  TEST_ASSERT(json.find("\"temp\": 21.3") != std::string::npos,
              "JSON contains Living Room zone temp");
  TEST_ASSERT(json.find("\"address\": \"32:137527\"") != std::string::npos,
              "JSON contains MVHR address");
  TEST_ASSERT(json.find("\"full_yaml\":") != std::string::npos,
              "JSON contains full_yaml field");
  TEST_ASSERT(json.find("\"yaml\":") != std::string::npos,
              "JSON contains per-device yaml field");
  TEST_ASSERT(json.find("\"packets\": [") != std::string::npos,
              "JSON contains live traffic packets list");
  TEST_ASSERT(json.find("\"opcode\": \"10E0\"") != std::string::npos,
              "JSON contains 10E0 packet");
  TEST_ASSERT(json.find("\"opcode_name\": \"Device Info / Signature\"") !=
                  std::string::npos,
              "JSON contains decoded opcode name");
}

void test_traffic_stream_and_foldout_decoding() {
  std::cout
      << "\n--- Testing Traffic Stream & Hopper D375 Fold-Out Decoding ---\n";
  RamsesDiscoveryComponent discovery;

  // Ingest Hopper D375 31DA telemetry packet
  discovery.on_message(parse_msg(
      "072  I --- 32:137527 63:262142 --:------ 31DA 030 "
      "00EE180800085C0000000000000000000000000000000000000000000000"));

  const auto &packets = discovery.get_recent_packets();
  TEST_ASSERT(!packets.empty(),
              "Packets list is populated in discovery engine");
  TEST_ASSERT(packets.front().opcode_hex == "31DA",
              "Latest packet opcode is 31DA");
  TEST_ASSERT(packets.front().opcode_name == "Ventilation Status & Telemetry",
              "Opcode name is Ventilation Status & Telemetry");

  bool has_supply_temp = false;
  for (const auto &f : packets.front().fields) {
    if (f.name == "Supply Temperature" &&
        f.value.find("°C") != std::string::npos) {
      has_supply_temp = true;
      break;
    }
  }
  TEST_ASSERT(has_supply_temp,
              "Decoded Supply Temperature field in fold-out breakdown");

  std::string json = discovery.generate_json(1000);
  TEST_ASSERT(json.find("\"packets\":") != std::string::npos,
              "JSON output contains packets array");
  TEST_ASSERT(json.find("Ventilation Status & Telemetry") != std::string::npos,
              "JSON contains Ventilation Status name");
  TEST_ASSERT(json.find("Supply Temperature") != std::string::npos,
              "JSON contains decoded field name");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Discovery Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_passive_discovery();
  test_yaml_generation();
  test_real_world_system_logs();
  test_real_world_opentherm_log();
  test_hopper_d375_discovery();
  test_json_generation_and_device_yaml();
  test_traffic_stream_and_foldout_decoding();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run
            << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
