#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_discovery/ramses_discovery.h"

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

static RamsesMessage parse_msg(const std::string &hgi80) {
  RamsesMessage msg;
  msg.from_hgi80(hgi80);
  return msg;
}

void test_passive_discovery() {
  std::cout << "\n--- Testing RamsesDiscovery Passive Device Ingestion ---\n";
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
  std::cout << "Generated YAML snippet:\n" << yaml << "\n";

  TEST_ASSERT(yaml.find("climate:") != std::string::npos, "YAML contains climate platform");
  TEST_ASSERT(yaml.find("name: \"Lounge Heating\"") != std::string::npos, "YAML contains 'Lounge Heating'");
  TEST_ASSERT(yaml.find("controller_address: \"01:145678\"") != std::string::npos, "YAML contains controller address");
  TEST_ASSERT(yaml.find("water_heater:") != std::string::npos, "YAML contains water_heater platform");
  TEST_ASSERT(yaml.find("fan:") != std::string::npos, "YAML contains fan platform");
  TEST_ASSERT(yaml.find("scheme: orcon") != std::string::npos, "YAML contains scheme: orcon");
  TEST_ASSERT(yaml.find("sensor:") != std::string::npos, "YAML contains sensor platform");
  TEST_ASSERT(yaml.find("binary_sensor:") != std::string::npos, "YAML contains binary_sensor platform");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Discovery Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_passive_discovery();
  test_yaml_generation();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
