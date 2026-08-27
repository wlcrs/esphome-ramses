#include "components/ramses_devices/ramses_climate.h"
#include "components/ramses_devices/ramses_devices.h"
#include "components/ramses_devices/ramses_fan.h"
#include "components/ramses_devices/ramses_water_heater.h"
#include "components/ramses_discovery/ramses_discovery.h"
#include "components/ramses_discovery/ramses_nvs_storage.h"
#include "esphome/core/application.h"
#include <cassert>
#include <iostream>

using namespace esphome;
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

void test_nvs_save_load_clear() {
  std::cout << "\n--- Testing RAMSES NVS Save / Load / Clear ---\n";
  auto &nvs = RamsesNvsStorage::instance();

  nvs.clear_config();
  TEST_ASSERT(!nvs.has_config(), "NVS has no config initially");
  TEST_ASSERT(nvs.load_config().empty(), "Loaded empty string initially");

  std::string test_json =
      R"({"version":1,"devices":[{"address":"01:145678","type":"controller","name":"Evohome"}]})";
  bool saved = nvs.save_config(test_json);
  TEST_ASSERT(saved, "Successfully saved config to NVS mock");
  TEST_ASSERT(nvs.has_config(), "NVS reports has_config() true");
  TEST_ASSERT(nvs.load_config() == test_json,
              "Loaded config matches saved string");

  nvs.clear_config();
  TEST_ASSERT(!nvs.has_config(), "Config erased successfully");
  TEST_ASSERT(nvs.load_config().empty(), "Loaded string is empty after clear");
}

void test_dynamic_entity_instantiation() {
  std::cout << "\n--- Testing Dynamic Entity Factory from JSON Config ---\n";
  auto &nvs = RamsesNvsStorage::instance();

  std::string complex_json = R"({
    "version": 1,
    "devices": [
      {
        "address": "01:145678",
        "type": "controller",
        "name": "Main Heating",
        "zones": [
          { "index": 0, "name": "Living Room", "climate": true, "temp": true, "setpoint": true },
          { "index": 1, "name": "Master Bed", "climate": true, "temp": true, "setpoint": true }
        ],
        "dhw": true
      },
      {
        "address": "32:155617",
        "type": "hvac",
        "name": "MVHR Unit",
        "scheme": "orcon",
        "fan": true,
        "co2": true,
        "indoor_humidity": true,
        "bypass_position": true,
        "filter_remaining_days": true,
        "filter_alarm": true
      },
      {
        "address": "04:089123",
        "type": "trv",
        "name": "Radiator TRV",
        "heat_demand": true,
        "battery_level": true,
        "battery_low": true
      },
      {
        "address": "10:045678",
        "type": "opentherm",
        "name": "Boiler Bridge",
        "opentherm_modulation": true,
        "flame_active": true,
        "flow_temperature": true
      },
      {
        "address": "13:123456",
        "type": "relay",
        "name": "Zone Relay",
        "relay_demand": true
      },
      {
        "address": "18:005612",
        "type": "sensor",
        "name": "Outside Sensor",
        "outdoor_temperature": true
      }
    ]
  })";

  size_t count = nvs.instantiate_from_json(complex_json, nullptr);
  TEST_ASSERT(count > 0, "Entities successfully instantiated");
  TEST_ASSERT(nvs.is_configured(), "NVS Storage marked as configured");
  TEST_ASSERT(nvs.get_entity_count() == count, "Entity count matches");
  TEST_ASSERT(nvs.is_device_configured("01:145678"),
              "Controller 01:145678 is configured");
  TEST_ASSERT(nvs.is_device_configured("32:155617"),
              "MVHR 32:155617 is configured");
  TEST_ASSERT(!nvs.is_device_configured("99:999999"),
              "Unknown 99:999999 is NOT configured");
  std::cout << "  Instantiated " << count
            << " dynamic Home Assistant entities.\n";
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running RAMSES NVS & Dynamic Entity Tests\n";
  std::cout << "========================================\n";

  test_nvs_save_load_clear();
  test_dynamic_entity_instantiation();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run
            << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
