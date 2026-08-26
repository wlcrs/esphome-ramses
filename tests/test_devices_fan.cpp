#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_devices/ramses_fan.h"

using namespace esphome;
using namespace esphome::fan;
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

void test_fan_rx_status() {
  std::cout << "\n--- Testing RamsesFan RX (Status Updates) ---\n";
  RamsesFan fan;
  fan.set_device_address("32:155617");
  fan.set_scheme(HvacScheme::ORCON);

  // Broadcast 22F1 Orcon Low (mode 2)
  RamsesMessage low_msg = parse_msg("045  I --- 32:155617 --:------ 32:155617 22F1 003 0002FF");
  fan.on_message(low_msg);
  TEST_ASSERT(fan.state == true, "Fan state is ON");
  TEST_ASSERT(fan.preset_mode == "low", "Fan preset mode is low");
  TEST_ASSERT(fan.speed == 33, "Fan speed is 33%");

  // Broadcast 22F1 Orcon Boost (mode 5)
  RamsesMessage boost_msg = parse_msg("045  I --- 32:155617 --:------ 32:155617 22F1 003 0005FF");
  fan.on_message(boost_msg);
  TEST_ASSERT(fan.state == true, "Fan state is ON");
  TEST_ASSERT(fan.preset_mode == "boost", "Fan preset mode is boost");
  TEST_ASSERT(fan.speed == 100, "Fan speed is 100%");
}

void test_fan_control_tx() {
  std::cout << "\n--- Testing RamsesFan Control (TX Commands) ---\n";
  RamsesFan fan;
  fan.set_device_address("32:155617");
  fan.set_scheme(HvacScheme::ORCON);

  // Set preset mode to boost
  auto call = fan.make_call();
  call.set_preset_mode("boost");
  call.perform();

  TEST_ASSERT(fan.state == true, "Fan state updated to ON");
  TEST_ASSERT(fan.preset_mode == "boost", "Fan preset mode updated to boost");
  TEST_ASSERT(fan.speed == 100, "Fan speed updated to 100%");

  // Verify write packet encoding
  RamsesAddress hgi_src = RamsesAddress::from_string("18:005612");
  RamsesAddress dev = RamsesAddress::from_string("32:155617");
  RamsesMessage boost_write = FanStatePayload::encode_write(hgi_src, dev, FanPresetMode::BOOST, HvacScheme::ORCON);
  TEST_ASSERT(boost_write.type == RAMSES_MSG_W, "Fan write message type is W");
  TEST_ASSERT(boost_write.opcode[0] == 0x22 && boost_write.opcode[1] == 0xF1, "Opcode is 22F1");
  TEST_ASSERT(boost_write.payload[0] == 0x00 && boost_write.payload[1] == 0x05, "Payload contains Orcon boost code 0x05");

  // Verify low write packet encoding
  RamsesMessage low_write = FanStatePayload::encode_write(hgi_src, dev, FanPresetMode::LOW, HvacScheme::ORCON);
  TEST_ASSERT(low_write.payload[0] == 0x00 && low_write.payload[1] == 0x02, "Payload contains Orcon low code 0x02");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Fan Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_fan_rx_status();
  test_fan_control_tx();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
