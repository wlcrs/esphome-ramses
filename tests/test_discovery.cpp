#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <vector>
#include <string>
#include <filesystem>
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_discovery/ramses_discovery.h"

namespace fs = std::filesystem;
using namespace esphome;
using namespace esphome::ramses_esp;
using namespace esphome::ramses_discovery;

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

static std::string extract_hgi80_frame(const std::string &line) {
  size_t hash_pos = line.find('#');
  std::string cleaned = (hash_pos != std::string::npos) ? line.substr(0, hash_pos) : line;

  size_t start = cleaned.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = cleaned.find_last_not_of(" \t\r\n");
  cleaned = cleaned.substr(start, end - start + 1);

  if (cleaned.empty()) return "";

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
    "fixtures/corpus/",
    "../fixtures/corpus/",
    "tests/fixtures/corpus/",
    "../../tests/fixtures/corpus/",
    "../ramses_rf/tests/tests_rf/data_driven/"
  };
  for (const auto &p : prefixes) {
    std::string full = p + rel_path;
    if (fs::exists(full)) return full;
  }
  return "";
}

void test_passive_discovery() {
  std::cout << "\n--- Testing RamsesDiscovery Synthetic Ingestion ---\n";
  RamsesDiscoveryComponent discovery;

  // 1. Controller packets
  discovery.on_message(parse_msg("045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801073A"));
  discovery.on_message(parse_msg("045  I --- 01:145678 --:------ 01:145678 2309 003 00079E"));
  discovery.on_message(parse_msg("045 RP --- 01:145678 18:005612 --:------ 0004 022 00004C6F756E67650000000000000000000000000000"));
  discovery.on_message(parse_msg("045  I --- 01:145678 --:------ 01:145678 1260 003 000837"));

  // 2. HVAC Fan packets
  discovery.on_message(parse_msg("045  I --- 32:155617 --:------ 32:155617 22F1 003 0002FF"));
  discovery.on_message(parse_msg("045 RP --- 32:155617 18:005612 --:------ 10E0 038 0000000000006700000000000000000000000000000000000000000000000000000000000000"));
  discovery.on_message(parse_msg("045 RP --- 32:155617 18:005612 --:------ 10D0 006 00B4B4C80000"));

  // 3. TRV packets
  discovery.on_message(parse_msg("045  I --- 04:089123 --:------ 01:145678 3150 002 0046"));
  discovery.on_message(parse_msg("045  I --- 04:089123 --:------ 04:089123 1060 003 005801"));

  // 4. OpenTherm packets
  discovery.on_message(parse_msg("045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000"));

  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.size() == 4, "Discovered 4 distinct devices");

  // Verify Controller
  TEST_ASSERT(devices.count("01:145678") == 1, "Controller 01:145678 discovered");
  const auto &ctl = devices.at("01:145678");
  TEST_ASSERT(ctl.device_type == "controller", "Device type is controller");
  TEST_ASSERT(ctl.zones.size() == 2, "Controller has 2 zones");
  TEST_ASSERT(ctl.zones.at(0).name == "Lounge", "Zone 0 name is 'Lounge'");
  TEST_ASSERT(ctl.has_dhw == true, "Controller has DHW enabled");

  // Verify HVAC Unit
  TEST_ASSERT(devices.count("32:155617") == 1, "HVAC Unit 32:155617 discovered");
  const auto &hvac = devices.at("32:155617");
  TEST_ASSERT(hvac.is_hvac == true, "HVAC flag is true");
  TEST_ASSERT(hvac.oem_name == "orcon", "OEM Scheme identified as 'orcon'");

  // Verify TRV
  TEST_ASSERT(devices.count("04:089123") == 1, "TRV 04:089123 discovered");
  const auto &trv = devices.at("04:089123");
  TEST_ASSERT(trv.device_type == "trv", "Device type is trv");

  // Verify OpenTherm
  TEST_ASSERT(devices.count("10:045678") == 1, "OpenTherm 10:045678 discovered");
  const auto &ot = devices.at("10:045678");
  TEST_ASSERT(ot.device_type == "opentherm", "Device type is opentherm");
}

void test_yaml_generation() {
  std::cout << "\n--- Testing Auto-Generated YAML Output ---\n";
  RamsesDiscoveryComponent discovery;

  discovery.on_message(parse_msg("045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801073A"));
  discovery.on_message(parse_msg("045 RP --- 01:145678 18:005612 --:------ 0004 022 00004C6F756E67650000000000000000000000000000"));
  discovery.on_message(parse_msg("045  I --- 01:145678 --:------ 01:145678 1260 003 000837"));
  discovery.on_message(parse_msg("045  I --- 32:155617 --:------ 32:155617 22F1 003 0002FF"));
  discovery.on_message(parse_msg("045 RP --- 32:155617 18:005612 --:------ 10E0 038 0000000000006700000000000000000000000000000000000000000000000000000000000000"));
  discovery.on_message(parse_msg("045  I --- 04:089123 --:------ 01:145678 3150 002 0046"));
  discovery.on_message(parse_msg("045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000"));

  std::string yaml = discovery.generate_yaml();

  TEST_ASSERT(yaml.find("climate:") != std::string::npos, "YAML contains climate platform");
  TEST_ASSERT(yaml.find("name: \"Lounge Heating\"") != std::string::npos, "YAML contains 'Lounge Heating'");
  TEST_ASSERT(yaml.find("controller_address: \"01:145678\"") != std::string::npos, "YAML contains controller address");
  TEST_ASSERT(yaml.find("water_heater:") != std::string::npos, "YAML contains water_heater platform");
  TEST_ASSERT(yaml.find("fan:") != std::string::npos, "YAML contains fan platform");
  TEST_ASSERT(yaml.find("scheme: orcon") != std::string::npos, "YAML contains scheme: orcon");
  TEST_ASSERT(yaml.find("sensor:") != std::string::npos, "YAML contains sensor platform");
  TEST_ASSERT(yaml.find("binary_sensor:") != std::string::npos, "YAML contains binary_sensor platform");
}

void test_real_world_system_logs() {
  std::cout << "\n--- Testing Discovery Against Real-World Evohome System Log ---\n";
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
    if (frame.empty()) continue;
    RamsesMessage msg;
    if (msg.from_hgi80(frame)) {
      discovery.on_message(msg);
      packet_count++;
    }
  }

  std::cout << "  Ingested " << packet_count << " packets from real system log.\n";
  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.size() >= 5, "Discovered multiple real-world devices");
  TEST_ASSERT(devices.count("01:145038") == 1, "Discovered controller 01:145038");

  const auto &ctl = devices.at("01:145038");
  TEST_ASSERT(ctl.device_type == "controller", "Identified as Evohome controller");

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
  TEST_ASSERT(yaml.find("climate:") != std::string::npos || yaml.find("sensor:") != std::string::npos,
              "Generated valid ESPHome configuration from real-world packet stream");
}

void test_real_world_opentherm_log() {
  std::cout << "\n--- Testing Discovery Against Real-World OpenTherm System Log ---\n";
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
    if (frame.empty()) continue;
    RamsesMessage msg;
    if (msg.from_hgi80(frame)) {
      discovery.on_message(msg);
    }
  }

  const auto &devices = discovery.get_devices();
  TEST_ASSERT(devices.count("10:048122") == 1, "Discovered OpenTherm Bridge 10:048122");
  const auto &ot = devices.at("10:048122");
  TEST_ASSERT(ot.device_type == "opentherm", "Identified as OpenTherm Bridge");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Discovery Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_passive_discovery();
  test_yaml_generation();
  test_real_world_system_logs();
  test_real_world_opentherm_log();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
