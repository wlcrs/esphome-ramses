#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "components/ramses_esp/ramses_decoder.h"

using namespace esphome::ramses_esp;

#define EXPECT_DECODES(decoder, payload, length)                               \
  do {                                                                         \
    assert(decoder(payload, length).has_value());                              \
  } while (0)

#define EXPECT_REJECTS(decoder, payload, length)                               \
  do {                                                                         \
    assert(!decoder(payload, length).has_value());                             \
  } while (0)

int main() {
  uint8_t payload[8] = {0x00, 0x08, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00};

  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return TemperaturePayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return TemperaturePayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return SetpointPayload::decode(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return SetpointPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return SystemModePayload::decode(data, length);
      },
      payload, 1);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return SystemModePayload::decode(data, length);
      },
      payload, 0);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return SystemSyncPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return SystemSyncPayload::decode(data, length);
      },
      payload, 0);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return HeatDemandPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return HeatDemandPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return ZoneNamePayload::decode(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return ZoneNamePayload::decode(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return ZoneStructurePayload::decode(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return ZoneStructurePayload::decode(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return ZoneRolePayload::decode(data, length);
      },
      payload, 5);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return ZoneRolePayload::decode(data, length);
      },
      payload, 4);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return FanStatePayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return FanStatePayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return FanBoostPayload::decode(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return FanBoostPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return VentilationInfoPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return VentilationInfoPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return AirQualityPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return AirQualityPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return FilterInfoPayload::decode(data, length);
      },
      payload, 4);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return FilterInfoPayload::decode(data, length);
      },
      payload, 3);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return DeviceBatteryPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return DeviceBatteryPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return DeviceInfoPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return DeviceInfoPayload::decode(data, length);
      },
      payload, 0);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return OpenThermPayload::decode(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return OpenThermPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return OutdoorTemperaturePayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return OutdoorTemperaturePayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return Co2SensorPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return Co2SensorPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return RelayDemandPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return RelayDemandPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return ContactSensorPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return ContactSensorPayload::decode(data, length);
      },
      payload, 1);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return DhwConfigPayload::decode(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return DhwConfigPayload::decode(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return DhwStatePayload::decode_temp(data, length);
      },
      payload, 3);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return DhwStatePayload::decode_temp(data, length);
      },
      payload, 2);
  EXPECT_DECODES(
      [](const uint8_t *data, size_t length) {
        return DhwStatePayload::decode_state(data, length);
      },
      payload, 2);
  EXPECT_REJECTS(
      [](const uint8_t *data, size_t length) {
        return DhwStatePayload::decode_state(data, length);
      },
      payload, 1);

  std::cout << "Payload structure contract passed.\n";
  return 0;
}