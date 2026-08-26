#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_devices/ramses_devices.h"
#include "tests/mock/mock_ramses_esp.h"

using namespace esphome;
using namespace esphome::ramses_esp;
using namespace esphome::ramses_devices;

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

// Test harness subclass to inspect published states without requiring full ESPHome core runner
class TestableRamsesSensor : public RamsesSensor {
 public:
  float last_state{0.0f};
  bool state_published{false};

  void publish_state(float state) {
    this->last_state = state;
    this->state_published = true;
  }
};

class TestableRamsesBinarySensor : public RamsesBinarySensor {
 public:
  bool last_state{false};
  bool state_published{false};

  void publish_state(bool state) {
    this->last_state = state;
    this->state_published = true;
  }
};

static RamsesMessage parse_msg(const std::string &hgi80) {
  RamsesMessage msg;
  msg.from_hgi80(hgi80);
  return msg;
}

void test_zone_temperature_sensor() {
  std::cout << "\n--- Testing RamsesSensor (Zone Temperature) ---\n";
  TestableRamsesSensor sensor_z0;
  sensor_z0.set_device_address("01:145678");
  sensor_z0.set_zone_index(0);
  sensor_z0.set_sensor_type(RamsesSensorType::ZONE_TEMPERATURE);

  TestableRamsesSensor sensor_z1;
  sensor_z1.set_device_address("01:145678");
  sensor_z1.set_zone_index(1);
  sensor_z1.set_sensor_type(RamsesSensorType::ZONE_TEMPERATURE);

  // Broadcast 30C9: Zone 0 = 21.20 C, Zone 1 = 19.50 C
  RamsesMessage msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801079E");
  
  sensor_z0.on_message(msg);
  TEST_ASSERT(sensor_z0.state_published, "Zone 0 temperature published");
  TEST_ASSERT(std::abs(sensor_z0.last_state - 21.20f) < 0.01f, "Zone 0 temperature is 21.20 C");

  sensor_z1.on_message(msg);
  TEST_ASSERT(sensor_z1.state_published, "Zone 1 temperature published");
  TEST_ASSERT(std::abs(sensor_z1.last_state - 19.50f) < 0.01f, "Zone 1 temperature is 19.50 C");
}

void test_zone_setpoint_and_demand_sensors() {
  std::cout << "\n--- Testing RamsesSensor (Setpoint & Heat Demand) ---\n";
  TestableRamsesSensor sp_sensor;
  sp_sensor.set_device_address("01:145678");
  sp_sensor.set_zone_index(0);
  sp_sensor.set_sensor_type(RamsesSensorType::ZONE_SETPOINT);

  RamsesMessage sp_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 2309 003 000834");
  sp_sensor.on_message(sp_msg);
  TEST_ASSERT(sp_sensor.state_published, "Setpoint state published");
  TEST_ASSERT(std::abs(sp_sensor.last_state - 21.00f) < 0.01f, "Setpoint is 21.00 C");

  TestableRamsesSensor demand_sensor;
  demand_sensor.set_device_address("04:089123");
  demand_sensor.set_zone_index(0);
  demand_sensor.set_sensor_type(RamsesSensorType::HEAT_DEMAND);

  RamsesMessage demand_msg = parse_msg("045  I --- 04:089123 --:------ 01:145678 3150 002 0046");
  demand_sensor.on_message(demand_msg);
  TEST_ASSERT(demand_sensor.state_published, "Heat demand published");
  TEST_ASSERT(std::abs(demand_sensor.last_state - 35.0f) < 0.1f, "Heat demand is 35.0%");
}

void test_environmental_and_opentherm_sensors() {
  std::cout << "\n--- Testing RamsesSensor (CO2, Outdoor Temp, OpenTherm) ---\n";
  TestableRamsesSensor co2_sensor;
  co2_sensor.set_device_address("32:155617");
  co2_sensor.set_sensor_type(RamsesSensorType::CO2);

  RamsesMessage co2_msg = parse_msg("045  I --- 32:155617 --:------ 32:155617 1298 003 000258");
  co2_sensor.on_message(co2_msg);
  TEST_ASSERT(co2_sensor.state_published, "CO2 sensor published");
  TEST_ASSERT(std::abs(co2_sensor.last_state - 600.0f) < 0.1f, "CO2 is 600 ppm");

  TestableRamsesSensor ot_mod_sensor;
  ot_mod_sensor.set_device_address("10:045678");
  ot_mod_sensor.set_sensor_type(RamsesSensorType::OPENTHERM_MODULATION);

  RamsesMessage ot_msg = parse_msg("045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000");
  ot_mod_sensor.on_message(ot_msg);
  TEST_ASSERT(ot_mod_sensor.state_published, "OpenTherm modulation published");
  TEST_ASSERT(std::abs(ot_mod_sensor.last_state - 40.0f) < 0.1f, "Modulation is 40.0%");
}

void test_binary_sensors() {
  std::cout << "\n--- Testing RamsesBinarySensor (Flame, Filter, Window, Battery) ---\n";
  TestableRamsesBinarySensor flame_sensor;
  flame_sensor.set_device_address("10:045678");
  flame_sensor.set_sensor_type(RamsesBinarySensorType::FLAME_ACTIVE);

  RamsesMessage ot_msg = parse_msg("045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000");
  flame_sensor.on_message(ot_msg);
  TEST_ASSERT(flame_sensor.state_published, "Flame active state published");
  TEST_ASSERT(flame_sensor.last_state == true, "Flame is active (true)");

  TestableRamsesBinarySensor filter_sensor;
  filter_sensor.set_device_address("32:155617");
  filter_sensor.set_sensor_type(RamsesBinarySensorType::FILTER_ALARM);

  RamsesMessage filter_dirty_msg = parse_msg("045  I --- 32:155617 --:------ 32:155617 10A0 002 0001");
  filter_sensor.on_message(filter_dirty_msg);
  TEST_ASSERT(filter_sensor.state_published, "Filter alarm state published");
  TEST_ASSERT(filter_sensor.last_state == true, "Filter dirty is true");

  TestableRamsesBinarySensor bat_low_sensor;
  bat_low_sensor.set_device_address("04:089123");
  bat_low_sensor.set_sensor_type(RamsesBinarySensorType::BATTERY_LOW);

  RamsesMessage bat_low_msg = parse_msg("045  I --- 04:089123 --:------ 04:089123 1060 003 001000");
  bat_low_sensor.on_message(bat_low_msg);
  TEST_ASSERT(bat_low_sensor.state_published, "Battery low state published");
  TEST_ASSERT(bat_low_sensor.last_state == true, "Battery is low (true)");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Devices Sensors & Binary Sensors Unit Tests\n";
  std::cout << "========================================\n";

  test_zone_temperature_sensor();
  test_zone_setpoint_and_demand_sensors();
  test_environmental_and_opentherm_sensors();
  test_binary_sensors();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
