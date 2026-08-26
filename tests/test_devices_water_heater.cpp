#include "components/ramses_devices/ramses_water_heater.h"
#include "components/ramses_esp/ramses_decoder.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace esphome;
using namespace esphome::water_heater;
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

static RamsesMessage parse_msg(const std::string &hgi80) {
  RamsesMessage msg;
  msg.from_hgi80(hgi80);
  return msg;
}

void test_water_heater_rx_temp_and_setpoint() {
  std::cout << "\n--- Testing RamsesWaterHeater RX (Temp & Setpoint) ---\n";
  RamsesWaterHeater dhw;
  dhw.set_controller_address("01:145678");

  // Broadcast 1260 (DHW Temp = 21.03 C)
  RamsesMessage temp_msg =
      parse_msg("045  I --- 01:145678 --:------ 01:145678 1260 003 000837");
  dhw.on_message(temp_msg);
  TEST_ASSERT(std::abs(dhw.get_current_temperature() - 21.03f) < 0.01f,
              "Current DHW temperature is 21.03 C");

  RamsesMessage unavailable_msg =
      parse_msg("045  I --- 01:145678 --:------ 01:145678 1260 003 007FFF");
  dhw.on_message(unavailable_msg);
  TEST_ASSERT(std::abs(dhw.get_current_temperature() - 21.03f) < 0.01f,
              "Unavailable DHW temperature preserves the last valid state");

  // Broadcast 12F0 (DHW flow rate = 50.00 L/min); it must not change the
  // target.
  RamsesMessage sp_msg =
      parse_msg("045  I --- 01:145678 --:------ 01:145678 12F0 003 001388");
  dhw.on_message(sp_msg);
  TEST_ASSERT(std::isnan(dhw.get_target_temperature()),
              "DHW flow rate does not set a target temperature");
}

void test_water_heater_rx_state() {
  std::cout << "\n--- Testing RamsesWaterHeater RX (Operating State) ---\n";
  RamsesWaterHeater dhw;
  dhw.set_controller_address("01:145678");

  // 1F41: DHW enabled, relay active (0x01 = active) -> PERFORMANCE mode
  RamsesMessage active_msg =
      parse_msg("045  I --- 01:145678 --:------ 01:145678 1F41 002 0001");
  dhw.on_message(active_msg);
  TEST_ASSERT(dhw.get_mode() == WATER_HEATER_MODE_PERFORMANCE,
              "DHW mode is PERFORMANCE (Relay active)");

  // 1F41: DHW enabled, relay inactive (0x00 = idle) -> ECO mode
  RamsesMessage idle_msg =
      parse_msg("045  I --- 01:145678 --:------ 01:145678 1F41 002 0000");
  dhw.on_message(idle_msg);
  TEST_ASSERT(dhw.get_mode() == WATER_HEATER_MODE_ECO,
              "DHW mode is ECO (Relay idle)");

  RamsesMessage disabled_msg = parse_msg(
      "045  I --- 01:145678 --:------ 01:145678 1F41 006 00FF00FFFFFF");
  dhw.on_message(disabled_msg);
  TEST_ASSERT(dhw.get_mode() == WATER_HEATER_MODE_OFF,
              "Extended DHW state disables the water heater");
}

void test_water_heater_control_tx() {
  std::cout << "\n--- Testing RamsesWaterHeater Control (TX Commands) ---\n";
  RamsesWaterHeater dhw;
  dhw.set_controller_address("01:145678");

  // Set DHW setpoint via WaterHeaterCall
  auto call = dhw.make_call();
  call.set_target_temperature(55.0f);
  call.perform();

  TEST_ASSERT(std::abs(dhw.get_target_temperature() - 55.0f) < 0.01f,
              "Target setpoint updated to 55.0 C");

  // Verify write packet encoding
  RamsesAddress hgi_src = RamsesAddress::from_string("18:005612");
  RamsesAddress ctl = RamsesAddress::from_string("01:145678");
  RamsesMessage dhw_write =
      DhwStatePayload::encode_write_setpoint(hgi_src, ctl, 55.0f);
  TEST_ASSERT(dhw_write.type == RAMSES_MSG_W, "DHW write message type is W");
  TEST_ASSERT(dhw_write.opcode[0] == 0x12 && dhw_write.opcode[1] == 0x60,
              "Opcode is 1260");
  TEST_ASSERT(dhw_write.payload[0] == 0x00, "Zone 00");
  uint16_t encoded_temp =
      (static_cast<uint16_t>(dhw_write.payload[1]) << 8) | dhw_write.payload[2];
  TEST_ASSERT(encoded_temp == 5500, "Encoded setpoint is 5500 (55.00 C)");

  RamsesMessage mode_write = DhwStatePayload::encode_write_mode(
      hgi_src, ctl, DhwStatePayload::OperationMode::PERMANENT_ON);
  TEST_ASSERT(mode_write.type == RAMSES_MSG_W && mode_write.opcode[0] == 0x1F &&
                  mode_write.opcode[1] == 0x41,
              "DHW mode write uses 1F41");
  TEST_ASSERT(mode_write.n_payload == 6 && mode_write.payload[1] == 0x01 &&
                  mode_write.payload[2] == 0x02,
              "DHW performance mode payload is encoded");

  RamsesMessage off_write = DhwStatePayload::encode_write_mode(
      hgi_src, ctl, DhwStatePayload::OperationMode::PERMANENT_OFF);
  TEST_ASSERT(off_write.payload[1] == 0x00 && off_write.payload[2] == 0x02,
              "DHW OFF mode payload is encoded");

  RamsesMessage eco_write = DhwStatePayload::encode_write_mode(
      hgi_src, ctl, DhwStatePayload::OperationMode::FOLLOW_SCHEDULE);
  TEST_ASSERT(eco_write.payload[1] == 0xFF && eco_write.payload[2] == 0x00,
              "DHW ECO mode follows the schedule");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Water Heater Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_water_heater_rx_temp_and_setpoint();
  test_water_heater_rx_state();
  test_water_heater_control_tx();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run
            << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
