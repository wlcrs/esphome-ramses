#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include "components/ramses_esp/ramses_decoder.h"

using namespace esphome::ramses_esp;

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

void test_temperature_codec_30c9() {
  std::cout << "\n--- Testing Opcode 0x30C9 (Temperatures) ---\n";
  // Raw multi-zone payload: Zone 0 = 21.20 C (0x0848), Zone 1 = 18.50 C (0x073A), Zone 2 = Sentinel 0x7FFF (Invalid)
  uint8_t payload[] = {0x00, 0x08, 0x48, 0x01, 0x07, 0x3A, 0x02, 0x7F, 0xFF};
  auto dec = TemperaturePayload::decode(payload, sizeof(payload));

  TEST_ASSERT(dec.has_value(), "Temperature payload decoded successfully");
  TEST_ASSERT(dec->zones.size() == 3, "Decoded 3 zone items");

  TEST_ASSERT(dec->zones[0].zone_index == 0, "Zone 0 index is 0");
  TEST_ASSERT(dec->zones[0].is_valid == true, "Zone 0 temperature is valid");
  TEST_ASSERT(std::abs(dec->zones[0].temperature - 21.20f) < 0.01f, "Zone 0 temp is 21.20 C");

  TEST_ASSERT(dec->zones[1].zone_index == 1, "Zone 1 index is 1");
  TEST_ASSERT(dec->zones[1].is_valid == true, "Zone 1 temperature is valid");
  TEST_ASSERT(std::abs(dec->zones[1].temperature - 18.50f) < 0.01f, "Zone 1 temp is 18.50 C");

  TEST_ASSERT(dec->zones[2].zone_index == 2, "Zone 2 index is 2");
  TEST_ASSERT(dec->zones[2].is_valid == false, "Zone 2 sentinel 0x7FFF recognized as invalid");
}

void test_temperature_edge_cases() {
  std::cout << "\n--- Testing Temperature Edge Cases (Negative & Sentinels) ---\n";
  
  // 1. Negative sub-zero temperature: -5.50 C = -550 = 0xFDDA
  uint8_t neg_payload[] = {0x00, 0xFD, 0xDA};
  auto dec_neg = TemperaturePayload::decode(neg_payload, sizeof(neg_payload));
  TEST_ASSERT(dec_neg.has_value(), "Negative temperature decoded");
  TEST_ASSERT(dec_neg->zones[0].is_valid == true, "Negative temperature is valid");
  TEST_ASSERT(std::abs(dec_neg->zones[0].temperature - (-5.50f)) < 0.01f, "Decoded negative temp is -5.50 C");

  // 2. Disabled sentinel 0x7EFF
  uint8_t dis_payload[] = {0x01, 0x7E, 0xFF};
  auto dec_dis = TemperaturePayload::decode(dis_payload, sizeof(dis_payload));
  TEST_ASSERT(dec_dis.has_value(), "Disabled sentinel payload decoded");
  TEST_ASSERT(dec_dis->zones[0].is_valid == false, "Sentinel 0x7EFF marked as invalid");

  // 3. Misaligned payload length (5 bytes: 1 complete zone + 2 dangling bytes)
  uint8_t misaligned[] = {0x00, 0x08, 0x34, 0x01, 0x07};
  auto dec_mis = TemperaturePayload::decode(misaligned, sizeof(misaligned));
  TEST_ASSERT(dec_mis.has_value(), "Misaligned payload handled safely");
  TEST_ASSERT(dec_mis->zones.size() == 1, "Only complete 3-byte zones unpacked");
  TEST_ASSERT(std::abs(dec_mis->zones[0].temperature - 21.00f) < 0.01f, "First zone unpacked correctly");

  // 4. Nullptr and zero length
  TEST_ASSERT(!TemperaturePayload::decode(nullptr, 10).has_value(), "Nullptr returns nullopt");
  TEST_ASSERT(!TemperaturePayload::decode(neg_payload, 0).has_value(), "0-length returns nullopt");
  uint8_t short_temp_payload[] = {0xFD, 0xDA};
  auto short_temp = TemperaturePayload::decode(short_temp_payload, sizeof(short_temp_payload));
  TEST_ASSERT(short_temp.has_value(), "2-byte temperature variant decoded");
  TEST_ASSERT(std::abs(short_temp->zones[0].temperature - (-5.50f)) < 0.01f,
              "2-byte temperature variant matches");
}

void test_setpoint_codec_2309() {
  std::cout << "\n--- Testing Opcode 0x2309 (Setpoints) ---\n";
  // Zone 0 setpoint = 21.50 C (0x0866)
  uint8_t payload[] = {0x00, 0x08, 0x66};
  auto dec = SetpointPayload::decode(payload, sizeof(payload));

  TEST_ASSERT(dec.has_value(), "Setpoint payload decoded successfully");
  TEST_ASSERT(dec->zones.size() == 1, "Decoded 1 zone setpoint");
  TEST_ASSERT(dec->zones[0].is_valid == true, "Setpoint is valid");
  TEST_ASSERT(std::abs(dec->zones[0].setpoint - 21.50f) < 0.01f, "Setpoint is 21.50 C");

  // Test encoding write command
  RamsesAddress src = RamsesAddress::from_string("18:005612");
  RamsesAddress dst = RamsesAddress::from_string("01:145678");
  RamsesMessage msg = SetpointPayload::encode_write(src, dst, 1, 20.0f);

  TEST_ASSERT(msg.type == RAMSES_MSG_W, "Encoded message type is W");
  TEST_ASSERT(msg.opcode[0] == 0x23 && msg.opcode[1] == 0x09, "Opcode is 2309");
  TEST_ASSERT(msg.payload[0] == 1, "Target zone index is 1");
  uint16_t enc_sp = ((uint16_t)msg.payload[1] << 8) | msg.payload[2];
  TEST_ASSERT(enc_sp == 2000, "Encoded 20.0 C as 2000 (0x07D0)");
}

void test_system_sync_codec_1f09() {
  std::cout << "\n--- Testing Opcode 0x1F09 (Synchronization Heartbeat) ---\n";
  uint8_t payload[] = {0x01, 0x07, 0xD0}; // Mode 1 = Heat off
  auto dec = SystemModePayload::decode(payload, sizeof(payload));

  TEST_ASSERT(dec.has_value(), "System sync decoded successfully");
  TEST_ASSERT(dec->mode == SystemMode::HEAT_OFF, "Decoded system mode as HEAT_OFF");
  TEST_ASSERT(std::string(system_mode_to_string(dec->mode)) == "heat_off", "String representation is 'heat_off'");

  // Unknown mode byte
  uint8_t unknown_payload[] = {0x09};
  auto dec_unk = SystemSyncPayload::decode(unknown_payload, sizeof(unknown_payload));
  TEST_ASSERT(dec_unk.has_value(), "Unknown mode byte decoded");
  TEST_ASSERT(dec_unk->mode == SystemMode::UNKNOWN, "Mode mapped to UNKNOWN");

  RamsesAddress src = RamsesAddress::from_string("18:005612");
  RamsesAddress dst = RamsesAddress::from_string("01:145678");
  RamsesMessage msg = SystemModePayload::encode_write(src, dst, SystemMode::ECO_BOOST);
  TEST_ASSERT(msg.opcode[0] == 0x2E && msg.opcode[1] == 0x04, "Encoded system mode uses opcode 2E04");
  TEST_ASSERT(msg.payload[0] == 2, "Encoded ECO mode as 2");
}

void test_system_mode_values_2e04() {
  std::cout << "\n--- Testing Opcode 0x2E04 System Mode Values ---\n";
  const SystemMode expected_modes[] = {
      SystemMode::AUTO, SystemMode::HEAT_OFF, SystemMode::ECO_BOOST, SystemMode::AWAY,
      SystemMode::DAY_OFF, SystemMode::DAY_OFF_ECO, SystemMode::AUTO_WITH_RESET,
  };
  for (uint8_t raw = 0; raw < 7; raw++) {
    uint8_t payload[] = {raw};
    auto dec = SystemModePayload::decode(payload, sizeof(payload));
    TEST_ASSERT(dec.has_value() && dec->mode == expected_modes[raw], "2E04 mode value decoded");
    TEST_ASSERT(dec->mode_raw == raw, "2E04 raw mode retained");
  }

  uint8_t unknown_payload[] = {0xFF};
  auto unknown = SystemModePayload::decode(unknown_payload, sizeof(unknown_payload));
  TEST_ASSERT(unknown.has_value() && unknown->mode == SystemMode::UNKNOWN, "2E04 unknown mode handled");
}

void test_heat_demand_codec_3150() {
  std::cout << "\n--- Testing Opcode 0x3150 (Heat Demand) ---\n";
  // Zone 0 demand: 100 / 200 = 50.0% (0x64)
  uint8_t payload[] = {0x00, 0x64};
  auto dec = HeatDemandPayload::decode(payload, sizeof(payload));

  TEST_ASSERT(dec.has_value(), "Heat demand decoded successfully");
  TEST_ASSERT(dec->domain_or_zone_index == 0, "Domain index is 0");
  TEST_ASSERT(std::abs(dec->demand_percent - 50.0f) < 0.1f, "Heat demand is 50.0%");

  // 0% demand (0x00) and 100% demand (200 = 0xC8)
  uint8_t zero_pl[] = {0x01, 0x00};
  TEST_ASSERT(std::abs(HeatDemandPayload::decode(zero_pl, 2)->demand_percent - 0.0f) < 0.01f, "0% demand decoded");
  uint8_t max_pl[] = {0x01, 0xC8};
  TEST_ASSERT(std::abs(HeatDemandPayload::decode(max_pl, 2)->demand_percent - 100.0f) < 0.01f, "100% demand decoded");
}

void test_zone_name_codec_0004() {
  std::cout << "\n--- Testing Opcode 0x0004 (Zone Name) ---\n";
  // 22B: [0x00, 0x00, 'L', 'o', 'u', 'n', 'g', 'e', 0, 0...]
  uint8_t payload[22] = {0x00, 0x00};
  const char *name_str = "Living Room";
  std::memcpy(&payload[2], name_str, std::strlen(name_str));

  auto dec = ZoneNamePayload::decode(payload, 22);
  TEST_ASSERT(dec.has_value(), "Zone name decoded successfully");
  TEST_ASSERT(dec->zone_index == 0, "Zone index is 0");
  TEST_ASSERT(dec->name == "Living Room", "Decoded name is 'Living Room'");

  // Uninitialized/deleted zone with 0x7F * 20
  uint8_t uninit_payload[22];
  uninit_payload[0] = 0x03;
  uninit_payload[1] = 0x00;
  std::memset(&uninit_payload[2], 0x7F, 20);
  auto dec_uninit = ZoneNamePayload::decode(uninit_payload, 22);
  TEST_ASSERT(dec_uninit.has_value(), "Deleted zone payload decoded");
  TEST_ASSERT(dec_uninit->name.empty(), "0x7F zone name decoded as empty string");

  RamsesAddress src = RamsesAddress::from_string("18:005612");
  RamsesAddress dst = RamsesAddress::from_string("01:145678");
  RamsesMessage query = ZoneNamePayload::encode_query(src, dst, 2);
  TEST_ASSERT(query.type == RAMSES_MSG_RQ, "Zone name query is RQ");
  TEST_ASSERT(query.payload[0] == 2, "Zone query requested index 2");
}

void test_hvac_fan_codec_22f1() {
  std::cout << "\n--- Testing Opcode 0x22F1 (HVAC Fan State & Mode) ---\n";
  // Orcon Medium mode: [0x00, 0x02, 0xFF]
  uint8_t payload_orcon[] = {0x00, 0x02, 0xFF};
  auto dec_orcon = FanStatePayload::decode(payload_orcon, sizeof(payload_orcon), HvacScheme::ORCON);
  TEST_ASSERT(dec_orcon.has_value(), "Orcon fan state decoded successfully");
  TEST_ASSERT(dec_orcon->preset_mode == FanPresetMode::MEDIUM, "Orcon mode 0x02 is MEDIUM");
  TEST_ASSERT(std::string(fan_preset_to_string(dec_orcon->preset_mode)) == "medium", "Preset name is 'medium'");

  // Orcon Boost mode: [0x00, 0x06, 0xFF]
  uint8_t payload_boost[] = {0x00, 0x06, 0xFF};
  auto dec_boost = FanStatePayload::decode(payload_boost, sizeof(payload_boost), HvacScheme::ORCON);
  TEST_ASSERT(dec_boost->preset_mode == FanPresetMode::BOOST, "Orcon mode 0x06 is BOOST");

  // Vasco Low mode: [0x00, 0x02, 0xFF]
  uint8_t payload_vasco[] = {0x00, 0x02, 0xFF};
  auto dec_vasco = FanStatePayload::decode(payload_vasco, sizeof(payload_vasco), HvacScheme::VASCO);
  TEST_ASSERT(dec_vasco->preset_mode == FanPresetMode::LOW, "Vasco mode 0x02 is LOW");

  // Itho mode index 2 -> LOW
  uint8_t payload_itho[] = {0x00, 0x02, 0xFF};
  auto dec_itho = FanStatePayload::decode(payload_itho, sizeof(payload_itho), HvacScheme::ITHO);
  TEST_ASSERT(dec_itho.has_value(), "Itho fan state decoded");
  TEST_ASSERT(dec_itho->preset_mode == FanPresetMode::LOW, "Itho mode index 2 is LOW");

  uint8_t payload_orcon_off[] = {0x00, 0x07, 0xFF};
  auto dec_orcon_off = FanStatePayload::decode(payload_orcon_off, sizeof(payload_orcon_off), HvacScheme::ORCON);
  TEST_ASSERT(dec_orcon_off->preset_mode == FanPresetMode::OFF, "Orcon mode 0x07 is OFF");

  uint8_t payload_vasco_auto[] = {0x00, 0x05, 0xFF};
  auto dec_vasco_auto = FanStatePayload::decode(payload_vasco_auto, sizeof(payload_vasco_auto), HvacScheme::VASCO);
  TEST_ASSERT(dec_vasco_auto->preset_mode == FanPresetMode::AUTO, "Vasco mode 0x05 is AUTO");

  auto dec_zehnder = FanStatePayload::decode(payload_vasco, sizeof(payload_vasco), HvacScheme::ZEHNDER);
  TEST_ASSERT(dec_zehnder->preset_mode == FanPresetMode::LOW, "Zehnder fallback mode 0x02 is LOW");

  // Unknown mode: 0xFF
  uint8_t payload_unk[] = {0x00, 0xFF, 0xFF};
  auto dec_unk = FanStatePayload::decode(payload_unk, sizeof(payload_unk), HvacScheme::ORCON);
  TEST_ASSERT(dec_unk->preset_mode == FanPresetMode::UNKNOWN, "Unknown mode handled safely");

  // Fan write command
  RamsesAddress src = RamsesAddress::from_string("18:005612");
  RamsesAddress dst = RamsesAddress::from_string("32:155617");
  RamsesMessage w_msg = FanStatePayload::encode_write(src, dst, FanPresetMode::HIGH, HvacScheme::ORCON);
  TEST_ASSERT(w_msg.type == RAMSES_MSG_W, "Fan command is W");
  TEST_ASSERT(w_msg.payload[1] == 0x03, "Orcon High mode encoded as 0x03");

  uint8_t boost_timer[] = {0x00, 0x00, 0x0A};
  auto boost_dec = FanBoostPayload::decode(boost_timer, sizeof(boost_timer));
  TEST_ASSERT(boost_dec.has_value() && boost_dec->minutes == 10, "22F3 boost timer decoded");

  uint8_t boost_timer_hours[] = {0x00, 0x40, 0x02};
  auto boost_hours_dec = FanBoostPayload::decode(boost_timer_hours, sizeof(boost_timer_hours));
  TEST_ASSERT(boost_hours_dec.has_value() && boost_hours_dec->minutes == 120, "22F3 hour timer decoded");

  uint8_t short_device_info[] = {0x00};
  auto short_info_dec = DeviceInfoPayload::decode(short_device_info, sizeof(short_device_info));
  TEST_ASSERT(short_info_dec.has_value() && short_info_dec->info_type == 0 && short_info_dec->oem_code == 0,
              "Short 10E0 device-info variant decoded");
}

void test_filter_and_battery_codecs() {
  std::cout << "\n--- Testing Opcodes 0x10D0 (Filter) & 0x1060 (Battery) ---\n";
  // Filter 6B layout: [0x00, 180 (0xB4), 180 (0xB4), 200 (0xC8), 0x00, 0x00]
  uint8_t filter_payload[] = {0x00, 0xB4, 0xB4, 0xC8, 0x00, 0x00};
  auto filter_dec = FilterInfoPayload::decode(filter_payload, sizeof(filter_payload));
  TEST_ASSERT(filter_dec.has_value(), "Filter info decoded");
  TEST_ASSERT(filter_dec->remaining_days == 180, "Filter remaining days is 180");
  TEST_ASSERT(filter_dec->lifetime_days == 180, "Filter lifetime days is 180");
  TEST_ASSERT(std::abs(filter_dec->remaining_percent - 100.0f) < 0.1f, "Filter remaining % is 100%");

  // Battery: [0x00, 88, 0x01]
  uint8_t bat_payload[] = {0x00, 88, 0x01};
  auto bat_dec = DeviceBatteryPayload::decode(bat_payload, sizeof(bat_payload));
  TEST_ASSERT(bat_dec.has_value(), "Battery payload decoded");
  TEST_ASSERT(bat_dec->battery_percent == 88, "Battery percent is 88%");
  TEST_ASSERT(bat_dec->battery_low == false, "Battery low flag is false");

  // Low battery: 15% and low flag
  uint8_t low_bat[] = {0x00, 15, 0x00};
  auto dec_low = DeviceBatteryPayload::decode(low_bat, sizeof(low_bat));
  TEST_ASSERT(dec_low->battery_low == true, "Low battery recognized");
}

void test_air_quality_12a0_edge_cases() {
  std::cout << "\n--- Testing Opcode 0x12A0 (Multi-Sensor Air Quality) ---\n";
  // Valid indoor sensor: index 0, 55% humidity, 21.50 C (2150 = 0x0866)
  uint8_t aq_payload[] = {0x00, 55, 0x08, 0x66};
  auto dec = AirQualityPayload::decode(aq_payload, sizeof(aq_payload));
  TEST_ASSERT(dec.has_value(), "Air quality decoded");
  TEST_ASSERT(dec->sensor_index == 0, "Sensor index is 0 (indoor)");
  TEST_ASSERT(dec->humidity.has_value() && *dec->humidity == 55.0f, "Humidity is 55%");
  TEST_ASSERT(dec->temperature.has_value() && std::abs(*dec->temperature - 21.50f) < 0.01f, "Temp is 21.50 C");

  // Humidity sensor missing/disabled (payload[1] = 0xFF > 100)
  uint8_t no_hum[] = {0x01, 0xFF, 0x08, 0x66};
  auto dec_no_hum = AirQualityPayload::decode(no_hum, sizeof(no_hum));
  TEST_ASSERT(dec_no_hum.has_value(), "Sensor without humidity decoded");
  TEST_ASSERT(!dec_no_hum->humidity.has_value(), "Out-of-range humidity (>100) suppressed");
  TEST_ASSERT(dec_no_hum->temperature.has_value(), "Temperature parsed successfully");
}

void test_opentherm_and_dhw_codecs() {
  std::cout << "\n--- Testing Opcodes 0x3220 (OpenTherm) & 0x1260 (DHW) ---\n";
  // OpenTherm Status + Modulation: [0x00, 0x08 (Flame on), 0x0E (Modulation DataId), 80 (40.0% mod), 0x00]
  uint8_t ot_mod[] = {0x00, 0x08, 0x0E, 80, 0x00};
  auto ot_dec = OpenThermPayload::decode(ot_mod, sizeof(ot_mod));
  TEST_ASSERT(ot_dec.has_value(), "OpenTherm modulation decoded");
  TEST_ASSERT(ot_dec->flame_active == true, "Flame active is true");
  TEST_ASSERT(std::abs(ot_dec->modulation_percent - 40.0f) < 0.1f, "Modulation is 40.0%");

  // OpenTherm Boiler Flow Temp (DataId 0x19 = 55.50 C: 55 = 0x37, 0.50 * 256 = 128 = 0x80)
  uint8_t ot_temp[] = {0x00, 0x08, 0x19, 55, 128};
  auto ot_temp_dec = OpenThermPayload::decode(ot_temp, sizeof(ot_temp));
  TEST_ASSERT(ot_temp_dec.has_value(), "OpenTherm flow temp decoded");
  TEST_ASSERT(ot_temp_dec->flow_temp.has_value(), "Flow temp present");
  TEST_ASSERT(std::abs(*ot_temp_dec->flow_temp - 55.50f) < 0.01f, "Flow temp is 55.50 C");

  // DHW temp: [0x00, 0x14, 0x50] = 52.00 C
  uint8_t dhw_payload[] = {0x00, 0x14, 0x50};
  auto dhw_dec = DhwStatePayload::decode_temp(dhw_payload, sizeof(dhw_payload));
  TEST_ASSERT(dhw_dec.has_value(), "DHW temp decoded");
  TEST_ASSERT(std::abs(dhw_dec->current_temp - 52.00f) < 0.05f, "DHW temp is 52.00 C");

  // DHW setpoint write
  RamsesAddress src = RamsesAddress::from_string("18:005612");
  RamsesAddress dst = RamsesAddress::from_string("01:145678");
  RamsesMessage dhw_write = DhwStatePayload::encode_write_setpoint(src, dst, 55.0f);
  TEST_ASSERT(dhw_write.type == RAMSES_MSG_W, "DHW write is W");
  TEST_ASSERT(dhw_write.opcode[0] == 0x12 && dhw_write.opcode[1] == 0x60, "DHW opcode is 1260");
  uint16_t enc_dhw_sp = ((uint16_t)dhw_write.payload[1] << 8) | dhw_write.payload[2];
  TEST_ASSERT(enc_dhw_sp == 5500, "Encoded 55.0 C DHW setpoint as 5500");
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Running Ramses Opcode Codecs & Edge Cases Unit Tests\n";
  std::cout << "========================================\n";

  test_temperature_codec_30c9();
  test_temperature_edge_cases();
  test_setpoint_codec_2309();
  test_system_sync_codec_1f09();
  test_system_mode_values_2e04();
  test_heat_demand_codec_3150();
  test_zone_name_codec_0004();
  test_hvac_fan_codec_22f1();
  test_filter_and_battery_codecs();
  test_air_quality_12a0_edge_cases();
  test_opentherm_and_dhw_codecs();

  std::cout << "\n========================================\n";
  std::cout << "Results: " << tests_passed << "/" << tests_run << " tests passed.\n";
  std::cout << "========================================\n";

  return (tests_passed == tests_run) ? 0 : 1;
}
