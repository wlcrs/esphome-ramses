#include "tests/mock/mock_ramses_esp.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace esphome::ramses_esp;

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

void test_mock_evohome_controller() {
  std::cout << "\n--- Testing Mock Evohome Controller ---\n";
  MockRamsesEsp hub;

  auto ctl = std::make_shared<MockEvohomeController>("01:145678");
  ctl->add_zone(0, "Lounge", 21.2f, 21.0f);
  ctl->add_zone(1, "Master Bed", 18.5f, 19.0f);
  hub.register_device(ctl);

  std::vector<std::string> received_hgi80;
  hub.set_packet_callback(
      [&](const RamsesMessage &msg, const std::string &hgi80) {
        received_hgi80.push_back(hgi80);
      });

  // 1. Test Query Zone 0 Name (RQ 0004) from Gateway 18:005612
  std::string rq_0004 = "045 RQ --- 18:005612 01:145678 --:------ 0004 001 00";
  hub.inject_hgi80(rq_0004);

  TEST_ASSERT(received_hgi80.size() == 1,
              "Received RP 0004 response from controller");
  RamsesMessage rp_msg;
  bool ok = rp_msg.from_hgi80(received_hgi80.back());
  TEST_ASSERT(ok, "Parsed RP 0004 response HGI80 line");
  TEST_ASSERT(rp_msg.type == RAMSES_MSG_RP, "Response is RP");
  TEST_ASSERT(rp_msg.n_payload == 22, "Zone name payload is 22 bytes");
  TEST_ASSERT(rp_msg.payload[0] == 0x00, "Zone index is 0");
  std::string name(reinterpret_cast<char *>(&rp_msg.payload[2]));
  TEST_ASSERT(name.rfind("Lounge", 0) == 0,
              "Zone name string matches 'Lounge'");

  // 2. Test Query Zone Structure (RQ 0005)
  received_hgi80.clear();
  std::string rq_0005 = "045 RQ --- 18:005612 01:145678 --:------ 0005 001 00";
  hub.inject_hgi80(rq_0005);

  TEST_ASSERT(received_hgi80.size() == 1, "Received RP 0005 response");
  rp_msg.from_hgi80(received_hgi80.back());
  TEST_ASSERT(rp_msg.payload[3] == 2, "2 active zones reported in RP 0005");

  // 3. Test Set Zone 1 Setpoint (W 2309: Set Zone 1 to 22.50 C = 2250 = 0x08CA)
  received_hgi80.clear();
  std::string w_2309 =
      "045  W --- 18:005612 01:145678 --:------ 2309 003 0108CA";
  hub.inject_hgi80(w_2309);

  TEST_ASSERT(received_hgi80.size() == 1,
              "Controller echoed I 2309 setpoint update");
  TEST_ASSERT(ctl->get_zones()[1].target_setpoint == 22.5f,
              "Controller updated Zone 1 setpoint to 22.5 C");

  // 4. Test Set System Mode (W 1F09: Set Mode to Away = 0x01)
  received_hgi80.clear();
  std::string w_1f09 = "045  W --- 18:005612 01:145678 --:------ 1F09 001 01";
  hub.inject_hgi80(w_1f09);

  TEST_ASSERT(received_hgi80.size() == 1,
              "Controller broadcasted I 1F09 mode change");
  TEST_ASSERT(ctl->get_system_mode() == 1,
              "Controller system mode updated to 1 (Away)");
}

void test_mock_mvhr_ventilator() {
  std::cout << "\n--- Testing Mock MVHR Ventilator (Orcon) ---\n";
  MockRamsesEsp hub;

  auto fan =
      std::make_shared<MockMvhrVentilator>("32:155617", MOCK_SCHEME_ORCON);
  hub.register_device(fan);

  std::vector<std::string> received_hgi80;
  hub.set_packet_callback(
      [&](const RamsesMessage &msg, const std::string &hgi80) {
        received_hgi80.push_back(hgi80);
      });

  // 1. Query Device Info / OEM signature (RQ 10E0)
  std::string rq_10e0 = "045 RQ --- 18:005612 32:155617 --:------ 10E0 001 00";
  hub.inject_hgi80(rq_10e0);

  TEST_ASSERT(received_hgi80.size() == 1, "Received RP 10E0 response from fan");
  RamsesMessage rp_msg;
  rp_msg.from_hgi80(received_hgi80.back());
  TEST_ASSERT(rp_msg.payload[6] == 0x67, "OEM signature byte is 0x67 (Orcon)");

  // 2. Set Fan Speed to High (W 22F1: Mode 0x04)
  received_hgi80.clear();
  std::string w_22f1 =
      "045  W --- 18:005612 32:155617 --:------ 22F1 003 0004FF";
  hub.inject_hgi80(w_22f1);

  TEST_ASSERT(received_hgi80.size() == 1,
              "Fan broadcasted I 22F1 speed update");
  TEST_ASSERT(fan->get_fan_mode() == 0x04, "Fan mode updated to 0x04 (High)");
}

void test_mock_trv_and_opentherm() {
  std::cout << "\n--- Testing Mock TRV & OpenTherm Bridge Telemetry ---\n";
  MockRamsesEsp hub;

  auto trv = std::make_shared<MockTrv>("04:089123");
  trv->set_battery_percent(88);
  trv->set_demand(0.45f);
  hub.register_device(trv);

  auto otb = std::make_shared<MockOpenThermBridge>("10:045678");
  otb->set_modulation(40.0f);
  hub.register_device(otb);

  std::vector<std::string> received_hgi80;
  hub.set_packet_callback(
      [&](const RamsesMessage &msg, const std::string &hgi80) {
        received_hgi80.push_back(hgi80);
      });

  trv->broadcast_telemetry(hub);
  TEST_ASSERT(received_hgi80.size() == 2,
              "TRV broadcasted 2 packets (demand 3150 and battery 1060)");

  received_hgi80.clear();
  otb->broadcast_telemetry(hub);
  TEST_ASSERT(received_hgi80.size() == 1,
              "OpenTherm bridge broadcasted 1 packet (telemetry 3220)");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Mock Subsystem Unit Tests\n";
  std::cout << "========================================\n";

  test_mock_evohome_controller();
  test_mock_mvhr_ventilator();
  test_mock_trv_and_opentherm();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run
            << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
