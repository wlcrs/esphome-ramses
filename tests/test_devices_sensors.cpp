#include "components/ramses_devices/ramses_devices.h"
#include "components/ramses_esp/ramses_decoder.h"
#include "tests/mock/mock_ramses_esp.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace esphome;
using namespace esphome::ramses_esp;
using namespace esphome::ramses_devices;

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

// Test harness wrapper to inspect published states without requiring full
// ESPHome core runner
class TestableSensor : public RamsesSensor {
public:
  TestableSensor(RamsesSensorType type) : RamsesSensor(type) {}

  float last_state{0.0f};
  bool state_published{false};

  void publish_state(float state) {
    this->last_state = state;
    this->state_published = true;
  }
};

class TestableBinarySensor : public RamsesBinarySensor {
public:
  TestableBinarySensor(RamsesBinarySensorType type)
      : RamsesBinarySensor(type) {}

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
  TestableSensor sensor_z0(RamsesSensorType::ZONE_TEMPERATURE);
  sensor_z0.set_device_address("01:145678");
  sensor_z0.set_zone_index(0);

  TestableSensor sensor_z1(RamsesSensorType::ZONE_TEMPERATURE);
  sensor_z1.set_device_address("01:145678");
  sensor_z1.set_zone_index(1);

  // Broadcast 30C9: Zone 0 = 21.20 C, Zone 1 = 19.50 C
  RamsesMessage msg = parse_msg(
      "045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801079E");

  sensor_z0.on_message(msg);
  TEST_ASSERT(sensor_z0.state_published, "Zone 0 temperature published");
  TEST_ASSERT(std::abs(sensor_z0.last_state - 21.20f) < 0.01f,
              "Zone 0 temperature is 21.20 C");

  sensor_z1.on_message(msg);
  TEST_ASSERT(sensor_z1.state_published, "Zone 1 temperature published");
  TEST_ASSERT(std::abs(sensor_z1.last_state - 19.50f) < 0.01f,
              "Zone 1 temperature is 19.50 C");
}

void test_zone_setpoint_and_demand_sensors() {
  std::cout << "\n--- Testing RamsesSensor (Setpoint & Heat Demand) ---\n";
  TestableSensor sp_sensor(RamsesSensorType::ZONE_SETPOINT);
  sp_sensor.set_device_address("01:145678");
  sp_sensor.set_zone_index(0);

  RamsesMessage sp_msg =
      parse_msg("045  I --- 01:145678 --:------ 01:145678 2309 003 000834");
  sp_sensor.on_message(sp_msg);
  TEST_ASSERT(sp_sensor.state_published, "Setpoint state published");
  TEST_ASSERT(std::abs(sp_sensor.last_state - 21.00f) < 0.01f,
              "Setpoint is 21.00 C");

  TestableSensor demand_sensor(RamsesSensorType::HEAT_DEMAND);
  demand_sensor.set_device_address("04:089123");
  demand_sensor.set_zone_index(0);

  RamsesMessage demand_msg =
      parse_msg("045  I --- 04:089123 --:------ 01:145678 3150 002 0046");
  demand_sensor.on_message(demand_msg);
  TEST_ASSERT(demand_sensor.state_published, "Heat demand published");
  TEST_ASSERT(std::abs(demand_sensor.last_state - 35.0f) < 0.1f,
              "Heat demand is 35.0%");
}

void test_environmental_and_opentherm_sensors() {
  std::cout
      << "\n--- Testing RamsesSensor (CO2, Outdoor Temp, OpenTherm) ---\n";
  TestableSensor co2_sensor(RamsesSensorType::CO2);
  co2_sensor.set_device_address("32:155617");

  RamsesMessage co2_msg =
      parse_msg("045  I --- 32:155617 --:------ 32:155617 1298 003 000258");
  co2_sensor.on_message(co2_msg);
  TEST_ASSERT(co2_sensor.state_published, "CO2 sensor published");
  TEST_ASSERT(std::abs(co2_sensor.last_state - 600.0f) < 0.1f,
              "CO2 is 600 ppm");

  TestableSensor ot_mod_sensor(RamsesSensorType::OPENTHERM_MODULATION);
  ot_mod_sensor.set_device_address("10:045678");

  RamsesMessage ot_msg =
      parse_msg("045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000");
  ot_mod_sensor.on_message(ot_msg);
  TEST_ASSERT(ot_mod_sensor.state_published, "OpenTherm modulation published");
  TEST_ASSERT(std::abs(ot_mod_sensor.last_state - 40.0f) < 0.1f,
              "Modulation is 40.0%");

  TestableSensor ot_flow_sensor(RamsesSensorType::OPENTHERM_FLOW_TEMP);
  ot_flow_sensor.set_device_address("10:045678");
  ot_flow_sensor.on_message(parse_msg(
      "045  I --- 10:045678 --:------ 10:045678 3220 005 0008193780"));
  TEST_ASSERT(ot_flow_sensor.state_published &&
                  std::abs(ot_flow_sensor.last_state - 55.5f) < 0.01f,
              "OpenTherm flow temperature is 55.5 C");

  TestableSensor ot_return_sensor(RamsesSensorType::OPENTHERM_RETURN_TEMP);
  ot_return_sensor.set_device_address("10:045678");
  ot_return_sensor.on_message(parse_msg(
      "045  I --- 10:045678 --:------ 10:045678 3220 005 00081C2D80"));
  TEST_ASSERT(ot_return_sensor.state_published &&
                  std::abs(ot_return_sensor.last_state - 45.5f) < 0.01f,
              "OpenTherm return temperature is 45.5 C");
}

void test_relay_and_hvac_diagnostic_sensors() {
  std::cout << "\n--- Testing RamsesSensor (Relay and HVAC Diagnostics) ---\n";
  TestableSensor relay_sensor(RamsesSensorType::RELAY_DEMAND);
  relay_sensor.set_device_address("13:123456");
  relay_sensor.set_relay_index(2);

  RamsesMessage other_relay =
      parse_msg("045  I --- 13:123456 --:------ 13:123456 0008 002 01C8");
  relay_sensor.on_message(other_relay);
  TEST_ASSERT(!relay_sensor.state_published, "Other relay demand is ignored");

  RamsesMessage relay =
      parse_msg("045  I --- 13:123456 --:------ 13:123456 0008 002 02C8");
  relay_sensor.on_message(relay);
  TEST_ASSERT(relay_sensor.state_published, "Selected relay demand published");
  TEST_ASSERT(std::abs(relay_sensor.last_state - 100.0f) < 0.1f,
              "Selected relay demand is 100%");

  TestableSensor filter_lifetime(RamsesSensorType::FILTER_LIFETIME_DAYS);
  filter_lifetime.set_device_address("32:155617");
  filter_lifetime.on_message(parse_msg(
      "045  I --- 32:155617 --:------ 32:155617 10D0 006 00B4B4C80000"));
  TEST_ASSERT(filter_lifetime.state_published &&
                  filter_lifetime.last_state == 180.0f,
              "Filter lifetime is 180 days");

  TestableSensor bypass_position(RamsesSensorType::BYPASS_POSITION);
  bypass_position.set_device_address("32:155617");
  bypass_position.on_message(
      parse_msg("045  I --- 32:155617 --:------ 32:155617 10A0 002 3201"));
  TEST_ASSERT(bypass_position.state_published &&
                  bypass_position.last_state == 50.0f,
              "Bypass position is 50%");
}

void test_binary_sensors() {
  std::cout << "\n--- Testing RamsesBinarySensor (Flame, Filter, Window, "
               "Battery) ---\n";
  TestableBinarySensor flame_sensor(RamsesBinarySensorType::FLAME_ACTIVE);
  flame_sensor.set_device_address("10:045678");

  RamsesMessage ot_msg =
      parse_msg("045  I --- 10:045678 --:------ 10:045678 3220 005 0008005000");
  flame_sensor.on_message(ot_msg);
  TEST_ASSERT(flame_sensor.state_published, "Flame active state published");
  TEST_ASSERT(flame_sensor.last_state == true, "Flame is active (true)");

  TestableBinarySensor fault_sensor(RamsesBinarySensorType::FAULT_ALARM);
  fault_sensor.set_device_address("10:045678");
  fault_sensor.on_message(parse_msg(
      "045  I --- 10:045678 --:------ 10:045678 3220 005 0001005000"));
  TEST_ASSERT(fault_sensor.state_published && fault_sensor.last_state,
              "OpenTherm fault state is active");

  TestableBinarySensor filter_sensor(RamsesBinarySensorType::FILTER_ALARM);
  filter_sensor.set_device_address("32:155617");

  RamsesMessage filter_dirty_msg =
      parse_msg("045  I --- 32:155617 --:------ 32:155617 10A0 002 0001");
  filter_sensor.on_message(filter_dirty_msg);
  TEST_ASSERT(filter_sensor.state_published, "Filter alarm state published");
  TEST_ASSERT(filter_sensor.last_state == true, "Filter dirty is true");

  TestableBinarySensor bypass_sensor(RamsesBinarySensorType::BYPASS_ACTIVE);
  bypass_sensor.set_device_address("32:155617");
  bypass_sensor.on_message(
      parse_msg("045  I --- 32:155617 --:------ 32:155617 10A0 002 3200"));
  TEST_ASSERT(bypass_sensor.state_published && bypass_sensor.last_state,
              "Bypass active state is true");

  TestableBinarySensor bat_low_sensor(RamsesBinarySensorType::BATTERY_LOW);
  bat_low_sensor.set_device_address("04:089123");

  RamsesMessage bat_low_msg =
      parse_msg("045  I --- 04:089123 --:------ 04:089123 1060 003 001000");
  bat_low_sensor.on_message(bat_low_msg);
  TEST_ASSERT(bat_low_sensor.state_published, "Battery low state published");
  TEST_ASSERT(bat_low_sensor.last_state == true, "Battery is low (true)");
}

void test_factory_creation() {
  std::cout << "\n--- Testing Factory Creation ---\n";
  auto *s = make_ramses_sensor(RamsesSensorType::ZONE_TEMPERATURE);
  TEST_ASSERT(s != nullptr, "make_ramses_sensor creates valid instance");
  TEST_ASSERT(s->matches_opcode(0x30C9), "Factory sensor matches 0x30C9");
  TEST_ASSERT(!s->matches_opcode(0x1234), "Factory sensor rejects 0x1234");
  delete s;

  auto *bs = make_ramses_binary_sensor(RamsesBinarySensorType::FLAME_ACTIVE);
  TEST_ASSERT(bs != nullptr,
              "make_ramses_binary_sensor creates valid instance");
  TEST_ASSERT(bs->matches_opcode(0x3220),
              "Factory binary sensor matches 0x3220");
  TEST_ASSERT(!bs->matches_opcode(0x1234),
              "Factory binary sensor rejects 0x1234");
  delete bs;
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Devices Sensors & Binary Sensors Unit Tests\n";
  std::cout << "========================================\n";

  test_zone_temperature_sensor();
  test_zone_setpoint_and_demand_sensors();
  test_environmental_and_opentherm_sensors();
  test_relay_and_hvac_diagnostic_sensors();
  test_binary_sensors();
  test_factory_creation();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run
            << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
