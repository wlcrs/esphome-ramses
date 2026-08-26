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

void test_binding_codec() {
  std::cout << "\n--- Testing 1FC9 Binding Codec ---\n";

  // Test decode of real-world Orcon DIS offer:
  // I --- 37:171871 --:------ 37:171871 1FC9 024
  //   0022F1969F5F 0022F3969F5F 6710E0969F5F 001FC9969F5F
  RamsesMessage offer_msg = parse_msg(
      "073  I --- 37:171871 --:------ 37:171871 1FC9 024 "
      "0022F1969F5F0022F3969F5F6710E0969F5F001FC9969F5F");
  auto offer_dec = BindingPayload::decode(offer_msg);
  TEST_ASSERT(offer_dec.has_value(), "1FC9 Offer decoded successfully");
  TEST_ASSERT(offer_dec->is_offer, "Decoded as Offer phase");
  TEST_ASSERT(!offer_dec->is_accept, "Not decoded as Accept phase");
  TEST_ASSERT(!offer_dec->is_confirm, "Not decoded as Confirm phase");
  TEST_ASSERT(offer_dec->bindings.size() == 4, "Offer has 4 binding tuples");
  TEST_ASSERT(offer_dec->bindings[0].opcode == 0x22F1, "First binding is 22F1");
  TEST_ASSERT(offer_dec->bindings[2].oem_code == 0x67, "10E0 binding has Orcon OEM code 0x67");

  // Test decode of Accept response:
  // W --- 32:155617 37:171871 --:------ 1FC9 012 0031D9825FE10031DA825FE1
  RamsesMessage accept_msg = parse_msg(
      "070  W --- 32:155617 37:171871 --:------ 1FC9 012 "
      "0031D9825FE10031DA825FE1");
  auto accept_dec = BindingPayload::decode(accept_msg);
  TEST_ASSERT(accept_dec.has_value(), "1FC9 Accept decoded successfully");
  TEST_ASSERT(accept_dec->is_accept, "Decoded as Accept phase");
  TEST_ASSERT(accept_dec->bindings.size() == 2, "Accept has 2 binding tuples (31D9, 31DA)");
  TEST_ASSERT(accept_dec->bindings[0].opcode == 0x31D9, "First accept binding is 31D9");
  TEST_ASSERT(accept_dec->bindings[1].opcode == 0x31DA, "Second accept binding is 31DA");

  // Test decode of Confirm:
  // I --- 37:171871 32:155617 --:------ 1FC9 001 00
  RamsesMessage confirm_msg = parse_msg("072  I --- 37:171871 32:155617 --:------ 1FC9 001 00");
  auto confirm_dec = BindingPayload::decode(confirm_msg);
  TEST_ASSERT(confirm_dec.has_value(), "1FC9 Confirm decoded successfully");
  TEST_ASSERT(confirm_dec->is_confirm, "Decoded as Confirm phase");
  TEST_ASSERT(confirm_dec->bindings.empty(), "Confirm has no binding tuples");

  // Test encode_offer for Orcon scheme
  RamsesAddress remote = RamsesAddress::from_string("37:005612");
  RamsesMessage encoded_offer = BindingPayload::encode_offer(remote, HvacScheme::ORCON);
  TEST_ASSERT(encoded_offer.type == RAMSES_MSG_I, "Encoded offer is I type");
  TEST_ASSERT(encoded_offer.opcode[0] == 0x1F && encoded_offer.opcode[1] == 0xC9, "Opcode is 1FC9");
  TEST_ASSERT(encoded_offer.n_payload == 24, "Offer payload is 24 bytes (4 tuples)");
  TEST_ASSERT(encoded_offer.payload[0] == 0x00, "First tuple OEM code is 0x00");
  TEST_ASSERT(encoded_offer.payload[1] == 0x22 && encoded_offer.payload[2] == 0xF1, "First tuple opcode is 22F1");
  TEST_ASSERT(encoded_offer.payload[12] == 0x67, "10E0 tuple OEM code is 0x67 (Orcon)");

  // Test encode_offer for Vasco scheme (OEM 0x66)
  RamsesMessage vasco_offer = BindingPayload::encode_offer(remote, HvacScheme::VASCO);
  TEST_ASSERT(vasco_offer.payload[12] == 0x66, "Vasco offer OEM code is 0x66");

  // Test encode_confirm
  RamsesAddress fan_addr = RamsesAddress::from_string("32:155617");
  RamsesMessage encoded_confirm = BindingPayload::encode_confirm(remote, fan_addr);
  TEST_ASSERT(encoded_confirm.type == RAMSES_MSG_I, "Encoded confirm is I type");
  TEST_ASSERT(encoded_confirm.n_payload == 1, "Confirm payload is 1 byte");
  TEST_ASSERT(encoded_confirm.payload[0] == 0x00, "Confirm payload is 0x00");
}

void test_pairing_handshake_simulation() {
  std::cout << "\n--- Testing 1FC9 Pairing Handshake Simulation ---\n";

  // Simulate a fan unit without a pre-known address (discovery pairing)
  RamsesFan fan;
  fan.set_scheme(HvacScheme::ORCON);
  fan.set_fake_remote_address("37:005612");

  // Simulate pairing start
  fan.start_pairing(30000);
  TEST_ASSERT(fan.is_pairing(), "Pairing mode is active after start_pairing()");

  // Simulate receiving a 1FC9 Accept from the HRU (Hopper D375 / Orcon)
  // W --- 32:137527 37:005612 --:------ 1FC9 012 0031D9825FE10031DA825FE1
  RamsesMessage accept_msg = parse_msg(
      "070  W --- 32:137527 37:005612 --:------ 1FC9 012 "
      "0031D9825FE10031DA825FE1");
  fan.on_message(accept_msg);

  // After receiving Accept, pairing should be complete
  TEST_ASSERT(!fan.is_pairing(), "Pairing mode stops after Accept received");
  TEST_ASSERT(fan.get_device_address().is_valid, "Device address discovered from Accept");
  TEST_ASSERT(fan.get_device_address().dev_class == 32, "Discovered device class is 32 (HRU)");

  // Test stop_pairing cancels active pairing
  RamsesFan fan2;
  fan2.set_scheme(HvacScheme::ORCON);
  fan2.start_pairing(30000);
  TEST_ASSERT(fan2.is_pairing(), "Pairing active after start");
  fan2.stop_pairing();
  TEST_ASSERT(!fan2.is_pairing(), "Pairing stopped after stop_pairing()");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Fan Platform Unit Tests\n";
  std::cout << "========================================\n";

  test_fan_rx_status();
  test_fan_control_tx();
  test_binding_codec();
  test_pairing_handshake_simulation();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}

