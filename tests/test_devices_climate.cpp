#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_devices/ramses_climate.h"

using namespace esphome;
using namespace esphome::climate;
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

static RamsesMessage parse_msg(const std::string &hgi80) {
  RamsesMessage msg;
  msg.from_hgi80(hgi80);
  return msg;
}

void test_climate_rx_temperature_and_setpoint() {
  std::cout << "\n--- Testing RamsesClimate RX (Temperature & Setpoint) ---\n";
  RamsesClimate zone0;
  zone0.set_controller_address("01:145678");
  zone0.set_zone_index(0);

  RamsesClimate zone1;
  zone1.set_controller_address("01:145678");
  zone1.set_zone_index(1);

  // Broadcast 30C9: Zone 0 = 21.20 C, Zone 1 = 18.50 C
  RamsesMessage temp_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 30C9 006 00084801073A");
  zone0.on_message(temp_msg);
  zone1.on_message(temp_msg);

  TEST_ASSERT(std::abs(zone0.current_temperature - 21.20f) < 0.01f, "Zone 0 current temperature is 21.20 C");
  TEST_ASSERT(std::abs(zone1.current_temperature - 18.50f) < 0.01f, "Zone 1 current temperature is 18.50 C");

  // Broadcast 2309: Zone 0 = 21.00 C
  RamsesMessage sp_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 2309 003 000834");
  zone0.on_message(sp_msg);
  TEST_ASSERT(std::abs(zone0.target_temperature - 21.00f) < 0.01f, "Zone 0 target setpoint is 21.00 C");
}

void test_climate_rx_system_mode_and_demand() {
  std::cout << "\n--- Testing RamsesClimate RX (System Mode & Heat Demand) ---\n";
  RamsesClimate climate;
  climate.set_controller_address("01:145678");
  climate.set_zone_index(0);

  // 2E04 System mode: AWAY mode (mode=3)
  RamsesMessage away_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 2E04 008 0300000000000000");
  climate.on_message(away_msg);
  TEST_ASSERT(climate.mode == CLIMATE_MODE_HEAT, "Climate mode is HEAT");
  TEST_ASSERT(climate.preset.has_value() && *climate.preset == CLIMATE_PRESET_AWAY, "Climate preset is AWAY");

  // 2E04 System mode: OFF mode (mode=1)
  RamsesMessage off_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 2E04 008 0100000000000000");
  climate.on_message(off_msg);
  TEST_ASSERT(climate.mode == CLIMATE_MODE_OFF, "Climate mode is OFF");

  // 3150 Heat demand: 45% -> action HEATING
  RamsesMessage demand_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 3150 002 005A");
  climate.on_message(demand_msg);
  TEST_ASSERT(climate.action == CLIMATE_ACTION_HEATING, "Climate action is HEATING");

  // 3150 Heat demand: 0% -> action IDLE
  RamsesMessage idle_msg = parse_msg("045  I --- 01:145678 --:------ 01:145678 3150 002 0000");
  climate.on_message(idle_msg);
  TEST_ASSERT(climate.action == CLIMATE_ACTION_IDLE, "Climate action is IDLE");
}

void test_climate_control_tx() {
  std::cout << "\n--- Testing RamsesClimate Control (TX Commands) ---\n";
  RamsesClimate climate;
  climate.set_controller_address("01:145678");
  climate.set_zone_index(0);

  // Test setpoint change via climate call
  auto call = climate.make_call();
  call.set_target_temperature(22.5f);
  call.perform();

  TEST_ASSERT(std::abs(climate.target_temperature - 22.5f) < 0.01f, "Target temp updated to 22.5 C");

  // Verify write packet encoding
  RamsesAddress hgi_src = RamsesAddress::from_string("18:005612");
  RamsesAddress ctl = RamsesAddress::from_string("01:145678");
  RamsesMessage sp_write = SetpointPayload::encode_write(hgi_src, ctl, 0, 22.5f);
  TEST_ASSERT(sp_write.type == RAMSES_MSG_W, "Setpoint write message type is W");
  TEST_ASSERT(sp_write.opcode[0] == 0x23 && sp_write.opcode[1] == 0x09, "Opcode is 2309");

  // Verify system mode write packet encoding
  RamsesMessage away_write = SystemModePayload::encode_write(hgi_src, ctl, SystemMode::AWAY);
  TEST_ASSERT(away_write.type == RAMSES_MSG_W, "System mode write message type is W");
  TEST_ASSERT(away_write.opcode[0] == 0x2E && away_write.opcode[1] == 0x04, "Opcode is 2E04");
  TEST_ASSERT(away_write.payload[0] == 0x03, "Payload has AWAY mode (0x03)");

  auto preset_call = climate.make_call();
  preset_call.set_preset(CLIMATE_PRESET_ECO);
  preset_call.perform();
  TEST_ASSERT(climate.preset.has_value() && *climate.preset == CLIMATE_PRESET_ECO,
              "ECO preset is applied locally");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Climate Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_climate_rx_temperature_and_setpoint();
  test_climate_rx_system_mode_and_demand();
  test_climate_control_tx();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
