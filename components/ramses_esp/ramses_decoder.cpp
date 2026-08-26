#include "ramses_decoder.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome {
namespace ramses_esp {

// ----------------------------------------------------------------------
// Helper: Convert signed fixed-point (0.01 C) to float
// Equivalent to ramses_tx/helpers.py: hex_to_temp()
// ----------------------------------------------------------------------
static inline bool parse_temperature_raw(int16_t raw, float &out_temp) {
  if (raw == RAMSES_TEMP_SENTINEL_INVALID ||
      raw == RAMSES_TEMP_SENTINEL_DISABLED) {
    return false;
  }
  out_temp = static_cast<float>(raw) / 100.0f;
  return true;
}

// ----------------------------------------------------------------------
// Opcode 0x30C9: Temperature Telemetry
// ramses_rf reference: ramses_rf/payloads/heating.py:TemperaturePayload /
// parse_30c9()
// ----------------------------------------------------------------------
using TemperatureSingleFmt = binary::Struct<"!h">;
using TemperatureZoneFmt = binary::Struct<"!Bh">;

std::optional<TemperaturePayload>
TemperaturePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < TemperatureSingleFmt::size)
    return std::nullopt;

  TemperaturePayload res;
  if (len == TemperatureSingleFmt::size) {
    auto [raw_temp] = *TemperatureSingleFmt::unpack(payload, len);
    float temp = 0.0f;
    bool valid = parse_temperature_raw(raw_temp, temp);
    res.zones.push_back(ZoneTemperatureItem{
        .zone_index = 0,
        .temperature = temp,
        .is_valid = valid,
    });
    return res;
  }

  // Multi-zone arrays: each zone item is 3 bytes: [zone_index (1B), temp (2B
  // signed BE int)]
  for (auto [zone_idx, raw_temp] :
       TemperatureZoneFmt::unpack_all(payload, len)) {
    float temp = 0.0f;
    bool valid = parse_temperature_raw(raw_temp, temp);
    res.zones.push_back(ZoneTemperatureItem{
        .zone_index = zone_idx,
        .temperature = temp,
        .is_valid = valid,
    });
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x2309: Zone Setpoints
// ramses_rf reference: ramses_rf/payloads/heating.py:SetpointPayload
// ----------------------------------------------------------------------
using SetpointFmt = binary::Struct<"!Bh">;

std::optional<SetpointPayload> SetpointPayload::decode(const uint8_t *payload,
                                                       size_t len) {
  if (payload == nullptr || len < SetpointFmt::size)
    return std::nullopt;

  SetpointPayload res;
  for (auto [zone_idx, raw_sp] : SetpointFmt::unpack_all(payload, len)) {
    float sp = 0.0f;
    bool valid = parse_temperature_raw(raw_sp, sp);
    res.zones.push_back(ZoneSetpointItem{
        .zone_index = zone_idx,
        .setpoint = sp,
        .is_valid = valid,
    });
  }
  return res;
}

RamsesMessage SetpointPayload::encode_write(const RamsesAddress &src,
                                            const RamsesAddress &dst,
                                            uint8_t zone_index,
                                            float setpoint) {
  int16_t raw_sp = static_cast<int16_t>(std::round(setpoint * 100.0f));
  return RamsesMessageBuilder::write()
      .from(src)
      .to(dst)
      .opcode(0x2309)
      .payload_packed<SetpointFmt>(zone_index, raw_sp);
}

// ----------------------------------------------------------------------
// Opcode 0x1F09: System Sync & Mode
// ramses_rf reference: ramses_rf/payloads/system.py:SystemSyncPayload
// ----------------------------------------------------------------------
const char *system_mode_to_string(SystemMode mode) {
  switch (mode) {
  case SystemMode::AUTO:
    return "auto";
  case SystemMode::HEAT_OFF:
    return "heat_off";
  case SystemMode::ECO_BOOST:
    return "eco_boost";
  case SystemMode::AWAY:
    return "away";
  case SystemMode::DAY_OFF:
    return "day_off";
  case SystemMode::DAY_OFF_ECO:
    return "day_off_eco";
  case SystemMode::AUTO_WITH_RESET:
    return "auto_with_reset";
  case SystemMode::CUSTOM:
    return "custom";
  default:
    return "unknown";
  }
}

SystemMode system_mode_from_string(const std::string &str) {
  if (str == "auto")
    return SystemMode::AUTO;
  if (str == "heat_off" || str == "off")
    return SystemMode::HEAT_OFF;
  if (str == "eco_boost" || str == "eco")
    return SystemMode::ECO_BOOST;
  if (str == "away")
    return SystemMode::AWAY;
  if (str == "day_off")
    return SystemMode::DAY_OFF;
  if (str == "day_off_eco")
    return SystemMode::DAY_OFF_ECO;
  if (str == "auto_with_reset")
    return SystemMode::AUTO_WITH_RESET;
  if (str == "custom")
    return SystemMode::CUSTOM;
  return SystemMode::UNKNOWN;
}

using SystemModeFmt = binary::Struct<"!B">;

std::optional<SystemModePayload>
SystemModePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < SystemModeFmt::size)
    return std::nullopt;

  SystemModePayload res;
  auto [mode_raw] = *SystemModeFmt::unpack(payload, len);
  res.mode_raw = mode_raw;
  res.mode = SystemMode::UNKNOWN;
  switch (res.mode_raw) {
  case 0:
    res.mode = SystemMode::AUTO;
    break;
  case 1:
    res.mode = SystemMode::HEAT_OFF;
    break;
  case 2:
    res.mode = SystemMode::ECO_BOOST;
    break;
  case 3:
    res.mode = SystemMode::AWAY;
    break;
  case 4:
    res.mode = SystemMode::DAY_OFF;
    break;
  case 5:
    res.mode = SystemMode::DAY_OFF_ECO;
    break;
  case 6:
    res.mode = SystemMode::AUTO_WITH_RESET;
    break;
  default:
    break;
  }
  return res;
}

RamsesMessage SystemModePayload::encode_write(const RamsesAddress &src,
                                              const RamsesAddress &dst,
                                              SystemMode mode) {
  uint8_t payload[8]{0};
  SystemModeFmt::pack(payload, static_cast<uint8_t>(mode));
  return RamsesMessageBuilder::write().from(src).to(dst).opcode(0x2E04).payload(
      payload, 8);
}

using SystemSync1Fmt = binary::Struct<"!B">;
using SystemSync3Fmt = binary::Struct<"!BH">;

std::optional<SystemSyncPayload>
SystemSyncPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < SystemSync1Fmt::size)
    return std::nullopt;

  SystemSyncPayload res;
  auto [mode_raw] = *SystemSync1Fmt::unpack(payload, len);
  res.mode_raw = mode_raw;
  switch (res.mode_raw) {
  case 0:
    res.mode = SystemMode::AUTO;
    break;
  case 1:
    res.mode = SystemMode::AWAY;
    break;
  case 2:
    res.mode = SystemMode::DAY_OFF;
    break;
  case 3:
    res.mode = SystemMode::CUSTOM;
    break;
  case 4:
    res.mode = SystemMode::ECO;
    break;
  case 5:
    res.mode = SystemMode::OFF;
    break;
  default:
    res.mode = SystemMode::UNKNOWN;
    break;
  }

  if (len >= SystemSync3Fmt::size) {
    auto [mode, remaining] = *SystemSync3Fmt::unpack(payload, len);
    res.remaining_raw = remaining;
  }
  return res;
}

RamsesMessage SystemSyncPayload::encode_write(const RamsesAddress &src,
                                              const RamsesAddress &dst,
                                              SystemMode mode) {
  return RamsesMessageBuilder::write()
      .from(src)
      .to(dst)
      .opcode(0x1F09)
      .payload_packed<SystemSync1Fmt>(static_cast<uint8_t>(mode));
}

// ----------------------------------------------------------------------
// Opcode 0x3150: Heat Demand
// ramses_rf reference: ramses_rf/payloads/heating.py:HeatDemandPayload
// ----------------------------------------------------------------------
using HeatDemandFmt = binary::Struct<"!BB">;

std::optional<HeatDemandPayload>
HeatDemandPayload::decode(const uint8_t *payload, size_t len) {
  auto fields = HeatDemandFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [zone, demand_raw] = *fields;
  HeatDemandPayload res;
  res.domain_or_zone_index = zone;
  // Value 0..200 maps to 0.0%..100.0%
  res.demand_percent = (static_cast<float>(demand_raw) / 200.0f) * 100.0f;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x0004: Zone Name
// ramses_rf reference: ramses_rf/payloads/heating.py:ZoneNamePayload
// (ZoneName22BPayload)
// ----------------------------------------------------------------------
using ZoneNameHeaderFmt = binary::Struct<"!BB">;
using ZoneNameQueryFmt = binary::Struct<"!B">;

std::optional<ZoneNamePayload> ZoneNamePayload::decode(const uint8_t *payload,
                                                       size_t len) {
  if (payload == nullptr || len < ZoneNameHeaderFmt::size + 1)
    return std::nullopt;

  auto [zone_idx, flag] = *ZoneNameHeaderFmt::unpack(payload, len);
  ZoneNamePayload res;
  res.zone_index = zone_idx;
  // 22B layout: [index (1B), flag (1B), ASCII chars up to 20B]
  size_t name_len = (len >= 22) ? 20 : (len - 2);
  const char *raw_chars = reinterpret_cast<const char *>(&payload[2]);

  // Find first null or non-printable character
  size_t actual_len = 0;
  while (actual_len < name_len && raw_chars[actual_len] != '\0' &&
         raw_chars[actual_len] >= 32 && raw_chars[actual_len] <= 126) {
    actual_len++;
  }
  res.name = std::string(raw_chars, actual_len);
  return res;
}

RamsesMessage ZoneNamePayload::encode_query(const RamsesAddress &src,
                                            const RamsesAddress &dst,
                                            uint8_t zone_index) {
  return RamsesMessageBuilder::query()
      .from(src)
      .to(dst)
      .opcode(0x0004)
      .payload_packed<ZoneNameQueryFmt>(zone_index);
}

// ----------------------------------------------------------------------
// Opcode 0x0005: Zone Structure
// ramses_rf reference: ramses_rf/payloads/heating.py:ZoneStructurePayload
// ----------------------------------------------------------------------
using ZoneStructureFmt = binary::Struct<"!BH">;
using ZoneStructureQueryFmt = binary::Struct<"!B">;

std::optional<ZoneStructurePayload>
ZoneStructurePayload::decode(const uint8_t *payload, size_t len) {
  auto fields = ZoneStructureFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [zone_type, active_mask] = *fields;
  ZoneStructurePayload res;
  res.zone_type = zone_type;
  res.active_zone_mask = active_mask;
  if (len >= 4) {
    res.zone_count = payload[3];
  } else {
    // Count active bits in mask
    res.zone_count = 0;
    for (int b = 0; b < 16; b++) {
      if (res.active_zone_mask & (1 << b))
        res.zone_count++;
    }
  }
  return res;
}

RamsesMessage ZoneStructurePayload::encode_query(const RamsesAddress &src,
                                                 const RamsesAddress &dst) {
  return RamsesMessageBuilder::query()
      .from(src)
      .to(dst)
      .opcode(0x0005)
      .payload_packed<ZoneStructureQueryFmt>(0x00);
}

// ----------------------------------------------------------------------
// Opcode 0x000C: Zone Role Bindings
// ramses_rf reference: ramses_rf/payloads/heating.py:ZoneRolePayload
// ----------------------------------------------------------------------
using ZoneRoleItemHeaderFmt = binary::Struct<"!BB">;

std::optional<ZoneRolePayload> ZoneRolePayload::decode(const uint8_t *payload,
                                                       size_t len) {
  if (payload == nullptr || len < 5)
    return std::nullopt;

  ZoneRolePayload res;
  std::span<const uint8_t> buf(payload, len);
  // Each entry is 5 bytes: [zone_idx (1B), role (1B), addr_bytes (3B)]
  for (size_t i = 0; i + 5 <= buf.size(); i += 5) {
    auto item_slice = buf.subspan(i, 5);
    auto [zone_idx, role] = *ZoneRoleItemHeaderFmt::unpack(item_slice);
    res.bindings.push_back(ZoneRoleBindingItem{
        .zone_index = zone_idx,
        .role = role,
        .device_address =
            RamsesAddress::from_bytes(item_slice.subspan(2).data()),
    });
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x22F1 / 0x22F3: HVAC Fan State & Control
// ramses_rf reference: ramses_rf/payloads/hvac.py:FanStatePayload &
// ramses_rf/models/hvac_schemas.py
// ----------------------------------------------------------------------
uint8_t get_hvac_oem_code(HvacScheme scheme) {
  switch (scheme) {
  case HvacScheme::VASCO:
    return 0x66;
  case HvacScheme::ITHO:
    return 0x01;
  case HvacScheme::ZEHNDER:
    return 0x02;
  case HvacScheme::AUTO:
  case HvacScheme::ORCON:
  default:
    return 0x67;
  }
}

const char *fan_preset_to_string(FanPresetMode mode) {
  switch (mode) {
  case FanPresetMode::AUTO:
    return "auto";
  case FanPresetMode::LOW:
    return "low";
  case FanPresetMode::MEDIUM:
    return "medium";
  case FanPresetMode::HIGH:
    return "high";
  case FanPresetMode::BOOST:
    return "boost";
  case FanPresetMode::AWAY:
    return "away";
  case FanPresetMode::OFF:
    return "off";
  default:
    return "unknown";
  }
}

using FanStateFmt = binary::Struct<"!BB">;
using FanState3Fmt = binary::Struct<"!BBB">;

std::optional<FanStatePayload>
FanStatePayload::decode(const uint8_t *payload, size_t len, HvacScheme scheme) {
  auto fields = FanStateFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [hdr, raw_mode] = *fields;

  FanStatePayload res;
  res.raw_mode = raw_mode;

  switch (scheme) {
  case HvacScheme::ORCON:
  default:
    // Orcon: 00=away, 01=low, 02=medium, 03=high, 04/05=auto, 06=boost, 07=off.
    switch (res.raw_mode) {
    case 0x00:
      res.preset_mode = FanPresetMode::AWAY;
      res.speed_percent = 15;
      break;
    case 0x01:
      res.preset_mode = FanPresetMode::LOW;
      res.speed_percent = 33;
      break;
    case 0x02:
      res.preset_mode = FanPresetMode::MEDIUM;
      res.speed_percent = 66;
      break;
    case 0x03:
      res.preset_mode = FanPresetMode::HIGH;
      res.speed_percent = 100;
      break;
    case 0x04:
    case 0x05:
      res.preset_mode = FanPresetMode::AUTO;
      res.speed_percent = 50;
      break;
    case 0x06:
      res.preset_mode = FanPresetMode::BOOST;
      res.speed_percent = 100;
      break;
    case 0x07:
      res.preset_mode = FanPresetMode::OFF;
      res.speed_percent = 0;
      break;
    default:
      res.preset_mode = FanPresetMode::UNKNOWN;
      break;
    }
    break;

  case HvacScheme::VASCO:
  case HvacScheme::ZEHNDER:
    switch (res.raw_mode) {
    case 0x00:
      res.preset_mode = FanPresetMode::OFF;
      res.speed_percent = 0;
      break;
    case 0x01:
      res.preset_mode = FanPresetMode::AWAY;
      res.speed_percent = 15;
      break;
    case 0x02:
      res.preset_mode = FanPresetMode::LOW;
      res.speed_percent = 33;
      break;
    case 0x03:
      res.preset_mode = FanPresetMode::MEDIUM;
      res.speed_percent = 66;
      break;
    case 0x04:
      res.preset_mode = FanPresetMode::HIGH;
      res.speed_percent = 100;
      break;
    case 0x05:
      res.preset_mode = FanPresetMode::AUTO;
      res.speed_percent = 50;
      break;
    default:
      res.preset_mode = FanPresetMode::UNKNOWN;
      break;
    }
    break;

  case HvacScheme::ITHO:
    // Itho uses a mode index: 00=off, 01=trickle, 02=low, 03=medium, 04=high.
    switch (res.raw_mode) {
    case 0x00:
      res.preset_mode = FanPresetMode::OFF;
      res.speed_percent = 0;
      break;
    case 0x01:
      res.preset_mode = FanPresetMode::LOW;
      res.speed_percent = 20;
      break;
    case 0x02:
      res.preset_mode = FanPresetMode::LOW;
      res.speed_percent = 33;
      break;
    case 0x03:
      res.preset_mode = FanPresetMode::MEDIUM;
      res.speed_percent = 66;
      break;
    case 0x04:
      res.preset_mode = FanPresetMode::HIGH;
      res.speed_percent = 100;
      break;
    default:
      res.preset_mode = FanPresetMode::UNKNOWN;
      break;
    }
    break;
  }

  return res;
}

RamsesMessage FanStatePayload::encode_write(const RamsesAddress &src,
                                            const RamsesAddress &dst,
                                            FanPresetMode mode,
                                            HvacScheme scheme) {
  uint8_t mode_byte = 0x00;
  switch (scheme) {
  case HvacScheme::AUTO:
  case HvacScheme::ORCON:
    switch (mode) {
    case FanPresetMode::AWAY:
      mode_byte = 0x00;
      break;
    case FanPresetMode::LOW:
      mode_byte = 0x01;
      break;
    case FanPresetMode::MEDIUM:
      mode_byte = 0x02;
      break;
    case FanPresetMode::HIGH:
      mode_byte = 0x03;
      break;
    case FanPresetMode::AUTO:
      mode_byte = 0x04;
      break;
    case FanPresetMode::BOOST:
      mode_byte = 0x06;
      break;
    case FanPresetMode::OFF:
      mode_byte = 0x07;
      break;
    default:
      break;
    }
    break;
  case HvacScheme::ITHO:
    switch (mode) {
    case FanPresetMode::OFF:
      mode_byte = 0x00;
      break;
    case FanPresetMode::LOW:
      mode_byte = 0x02;
      break;
    case FanPresetMode::MEDIUM:
      mode_byte = 0x03;
      break;
    case FanPresetMode::HIGH:
    case FanPresetMode::BOOST:
      mode_byte = 0x04;
      break;
    default:
      break;
    }
    break;
  case HvacScheme::VASCO:
  case HvacScheme::ZEHNDER:
    switch (mode) {
    case FanPresetMode::OFF:
      mode_byte = 0x00;
      break;
    case FanPresetMode::AWAY:
      mode_byte = 0x01;
      break;
    case FanPresetMode::LOW:
      mode_byte = 0x02;
      break;
    case FanPresetMode::MEDIUM:
      mode_byte = 0x03;
      break;
    case FanPresetMode::HIGH:
    case FanPresetMode::BOOST:
      mode_byte = 0x04;
      break;
    case FanPresetMode::AUTO:
      mode_byte = 0x05;
      break;
    default:
      break;
    }
    break;
  }

  return RamsesMessageBuilder::write()
      .from(src)
      .to(dst)
      .opcode(0x22F1)
      .payload_packed<FanState3Fmt>(0x00, mode_byte, 0xFF);
}

using FanBoostFmt = binary::Struct<"!BBB">;

std::optional<FanBoostPayload> FanBoostPayload::decode(const uint8_t *payload,
                                                       size_t len) {
  auto fields = FanBoostFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [header, flags, minutes] = *fields;
  FanBoostPayload res;
  res.header = header;
  res.flags = flags;
  res.minutes = minutes;
  if (res.flags & 0x40)
    res.minutes = static_cast<uint16_t>(res.minutes * 60);
  return res;
}

RamsesMessage FanBoostPayload::encode_write(const RamsesAddress &src,
                                            const RamsesAddress &dst,
                                            uint16_t minutes) {
  uint8_t flags = 0x00;
  uint8_t min_byte = 0;
  if (minutes > 255 && minutes % 60 == 0 && minutes / 60 <= 255) {
    flags = 0x40;
    min_byte = static_cast<uint8_t>(minutes / 60);
  } else {
    flags = 0x00;
    min_byte = static_cast<uint8_t>(
        std::min<uint16_t>(minutes, static_cast<uint16_t>(255)));
  }

  return RamsesMessageBuilder::write()
      .from(src)
      .to(dst)
      .opcode(0x22F3)
      .payload_packed<FanBoostFmt>(0x00, flags, min_byte);
}

// ----------------------------------------------------------------------
// Opcode 0x10A0 / 0x22E5: Ventilation Info & Bypass Damper
// ramses_rf reference: ramses_rf/payloads/hvac.py:VentilationPayload
// ----------------------------------------------------------------------
using VentilationInfoFmt = binary::Struct<"!BB">;

std::optional<VentilationInfoPayload>
VentilationInfoPayload::decode(const uint8_t *payload, size_t len) {
  auto fields = VentilationInfoFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [bypass_pos, status_flags] = *fields;
  VentilationInfoPayload res;
  res.bypass_position = bypass_pos;
  res.bypass_active = (res.bypass_position > 0);
  res.filter_dirty = (status_flags & 0x01) != 0;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x12A0: Multi-Sensor Array (Air Quality, Humidity, Temps)
// ramses_rf reference: ramses_rf/payloads/hvac.py:AirQualityPayload &
// ramses_rf/quirks.py
// ----------------------------------------------------------------------
using AirQuality2Fmt = binary::Struct<"!BB">;
using AirQuality4Fmt = binary::Struct<"!BBh">;

std::optional<AirQualityPayload>
AirQualityPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < AirQuality2Fmt::size)
    return std::nullopt;

  AirQualityPayload res;
  if (len == AirQuality2Fmt::size) {
    auto [s_idx, hum] = *AirQuality2Fmt::unpack(payload, len);
    res.sensor_index = s_idx;
    if (hum <= 100)
      res.humidity = static_cast<float>(hum);
    return res;
  }

  if (len < AirQuality4Fmt::size)
    return std::nullopt;

  auto [s_idx, hum, raw_t] = *AirQuality4Fmt::unpack(payload, len);
  res.sensor_index = s_idx;
  if (hum <= 100) {
    res.humidity = static_cast<float>(hum);
  }

  float t_val = 0.0f;
  if (parse_temperature_raw(raw_t, t_val)) {
    res.temperature = t_val;
  }

  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x10D0: Filter Life Remaining
// ramses_rf reference: ramses_rf/payloads/hvac.py:FilterInfoPayload
// ----------------------------------------------------------------------
using FilterFmt = binary::Struct<"!xBBB">;

std::optional<FilterInfoPayload>
FilterInfoPayload::decode(const uint8_t *payload, size_t len) {
  auto fields = FilterFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [remaining_days, total_days, raw_pct] = *fields;

  FilterInfoPayload res;
  res.remaining_days = remaining_days;
  res.lifetime_days = total_days;
  res.remaining_percent = (static_cast<float>(raw_pct) / 200.0f) * 100.0f;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1060: Device Battery & State
// ramses_rf reference: ramses_rf/payloads/helpers.py
// ----------------------------------------------------------------------
using DeviceBatteryFmt = binary::Struct<"!BB">;

std::optional<DeviceBatteryPayload>
DeviceBatteryPayload::decode(const uint8_t *payload, size_t len) {
  auto fields = DeviceBatteryFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [domain_or_device, battery_pct] = *fields;
  DeviceBatteryPayload res;
  res.domain_or_device = domain_or_device;
  res.battery_percent = battery_pct;
  if (len >= 3) {
    res.battery_low = (payload[2] == 0x00 || battery_pct < 20);
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x10E0: Device Info & OEM Signature
// ramses_rf reference: ramses_rf/protocol/fingerprints.py & binding_fsm.py
// ----------------------------------------------------------------------
using DeviceInfoQueryFmt = binary::Struct<"!B">;

std::optional<DeviceInfoPayload>
DeviceInfoPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 1)
    return std::nullopt;

  DeviceInfoPayload res;
  res.info_type = payload[0];
  if (len < 7)
    return res;
  res.oem_code = payload[6]; // Standard offset for short OEM vendor byte
  if (len >= 8 && (res.oem_code == 0 || res.oem_code == 0x0A ||
                   res.oem_code == 0xFF || payload[7] == 0x6A)) {
    res.oem_code = payload[7]; // Extended OEM vendor byte (e.g. Brofer / Hopper
                               // D375: 0x6A)
  }
  return res;
}

RamsesMessage DeviceInfoPayload::encode_query(const RamsesAddress &src,
                                              const RamsesAddress &dst) {
  return RamsesMessageBuilder::query()
      .from(src)
      .to(dst)
      .opcode(0x10E0)
      .payload_packed<DeviceInfoQueryFmt>(0x00);
}

// ----------------------------------------------------------------------
// Opcode 0x3220: OpenTherm Telemetry
// ramses_rf reference: ramses_rf/payloads/opentherm.py:OpenthermPayload
// ----------------------------------------------------------------------
using OpenThermFmt = binary::Struct<"!BB">;

std::optional<OpenThermPayload> OpenThermPayload::decode(const uint8_t *payload,
                                                         size_t len) {
  if (payload == nullptr || len < 3)
    return std::nullopt;

  auto fields = OpenThermFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [msg_id, status] = *fields;
  OpenThermPayload res;
  res.msg_id = msg_id;
  res.flame_active = (status & 0x08) != 0;
  res.fault_active = (status & 0x01) != 0;

  if (len >= 5) {
    uint8_t data_id = payload[2];
    uint8_t u8_hi = payload[3];
    uint8_t u8_lo = payload[4];

    // Data ID 0x0E: Relative Modulation Level (0..200 -> 0.0..100.0%)
    if (data_id == 0x0E) {
      res.modulation_percent = static_cast<float>(u8_hi) / 2.0f;
    }
    // Data ID 0x19: Boiler Flow Temperature (f8.8 float)
    else if (data_id == 0x19) {
      res.flow_temp =
          static_cast<float>(u8_hi) + (static_cast<float>(u8_lo) / 256.0f);
    }
    // Data ID 0x1C: Return Water Temperature (f8.8 float)
    else if (data_id == 0x1C) {
      res.return_temp =
          static_cast<float>(u8_hi) + (static_cast<float>(u8_lo) / 256.0f);
    }
    // Fallback: modulation in standard Honeywell OTB status frame
    else if (data_id == 0x00 && u8_hi > 0) {
      res.modulation_percent = static_cast<float>(u8_hi) / 2.0f;
    }
  } else if (len >= 4) {
    res.modulation_percent = static_cast<float>(payload[3]) / 2.0f;
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1260 / 0x1F41: DHW State & Setpoints
// ramses_rf reference: ramses_rf/payloads/dhw.py:DhwStatePayload &
// DhwTempPayload
// ----------------------------------------------------------------------
using DhwSetpointFmt = binary::Struct<"!Bh">;
using DhwModeFmt = binary::Struct<"!BBBBBB">;

std::optional<DhwStatePayload>
DhwStatePayload::decode_temp(const uint8_t *payload, size_t len) {
  auto fields = DhwSetpointFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [zone_idx, raw_t] = *fields;
  DhwStatePayload res;
  res.current_temp_valid = parse_temperature_raw(raw_t, res.current_temp);
  return res;
}

// Opcode 0x1F41: DHW State
// Equivalent to: ramses_rf/payloads/dhw.py:DhwState2BPayload /
// DhwState3BPayload
std::optional<DhwStatePayload>
DhwStatePayload::decode_state(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2)
    return std::nullopt;

  DhwStatePayload res;
  uint8_t active_flag = payload[1];
  res.dhw_enabled = (active_flag != 0xFF);
  res.relay_active = (active_flag == 0x01);
  return res;
}

RamsesMessage DhwStatePayload::encode_write_setpoint(const RamsesAddress &src,
                                                     const RamsesAddress &dst,
                                                     float setpoint) {
  int16_t raw_sp = static_cast<int16_t>(std::round(setpoint * 100.0f));
  return RamsesMessageBuilder::write()
      .from(src)
      .to(dst)
      .opcode(0x1260)
      .payload_packed<DhwSetpointFmt>(0x00, raw_sp);
}

RamsesMessage DhwStatePayload::encode_write_mode(const RamsesAddress &src,
                                                 const RamsesAddress &dst,
                                                 OperationMode mode) {
  uint8_t p1 = mode == OperationMode::FOLLOW_SCHEDULE ? 0xFF
               : mode == OperationMode::PERMANENT_ON  ? 0x01
                                                      : 0x00;
  uint8_t p2 = mode == OperationMode::FOLLOW_SCHEDULE ? 0x00
               : mode == OperationMode::TEMPORARY_ON  ? 0x04
                                                      : 0x02;
  return RamsesMessageBuilder::write()
      .from(src)
      .to(dst)
      .opcode(0x1F41)
      .payload_packed<DhwModeFmt>(0x00, p1, p2, 0xFF, 0xFF, 0xFF);
}

// ----------------------------------------------------------------------
// Opcode 0x12C0: Outdoor Temperature
// ramses_rf reference: ramses_rf/payloads/heating.py:OutdoorTempPayload
// ----------------------------------------------------------------------
using OutdoorTemp3Fmt = binary::Struct<"!BBB">;
using OutdoorTemp2Fmt = binary::Struct<"!h">;

std::optional<OutdoorTemperaturePayload>
OutdoorTemperaturePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < OutdoorTemp2Fmt::size)
    return std::nullopt;

  OutdoorTemperaturePayload res;
  if (len >= OutdoorTemp3Fmt::size && payload[0] == 0x00) {
    auto [hdr, val, scale] = *OutdoorTemp3Fmt::unpack(payload, len);
    if (val == 0x80)
      return res;
    if (scale == 0x01) {
      res.temperature = static_cast<float>(val) / 2.0f;
    } else {
      res.temperature =
          std::round((static_cast<float>(val) - 32.0f) * 5.0f / 9.0f * 100.0f) /
          100.0f;
    }
    res.is_valid = true;
    return res;
  }
  auto [raw_t] = *OutdoorTemp2Fmt::unpack(payload, len);
  res.is_valid = parse_temperature_raw(raw_t, res.temperature);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1298: CO2 Sensor Telemetry
// ramses_rf reference: ramses_rf/payloads/hvac.py:Co2Payload
// ----------------------------------------------------------------------
using Co2Sensor2Fmt = binary::Struct<"!H">;
using Co2Sensor3Fmt = binary::Struct<"!xH">;

std::optional<Co2SensorPayload> Co2SensorPayload::decode(const uint8_t *payload,
                                                         size_t len) {
  if (payload == nullptr || len < Co2Sensor2Fmt::size)
    return std::nullopt;

  Co2SensorPayload res;
  if (len == Co2Sensor2Fmt::size) {
    auto [ppm] = *Co2Sensor2Fmt::unpack(payload, len);
    res.co2_ppm = ppm;
  } else {
    auto [ppm] = *Co2Sensor3Fmt::unpack(payload, len);
    res.co2_ppm = ppm;
  }
  res.is_valid = (res.co2_ppm < 0x7FFF);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x0008: Relay / Actuator Demand
// ramses_rf reference: ramses_rf/payloads/heating.py:RelayDemandPayload
// ----------------------------------------------------------------------
using RelayDemandFmt = binary::Struct<"!BB">;

std::optional<RelayDemandPayload>
RelayDemandPayload::decode(const uint8_t *payload, size_t len) {
  auto fields = RelayDemandFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [relay_idx, demand_raw] = *fields;
  RelayDemandPayload res;
  res.relay_index = relay_idx;
  res.demand_percent = (static_cast<float>(demand_raw) / 200.0f) * 100.0f;
  res.is_active = (demand_raw > 0);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x12B0: Window / Door Contact Sensor
// ramses_rf reference: ramses_rf/payloads/heating.py:WindowStatePayload
// ----------------------------------------------------------------------
using ContactSensorFmt = binary::Struct<"!BB">;

std::optional<ContactSensorPayload>
ContactSensorPayload::decode(const uint8_t *payload, size_t len) {
  auto fields = ContactSensorFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [zone_idx, state] = *fields;
  ContactSensorPayload res;
  res.zone_index = zone_idx;
  res.is_open = (state != 0x00);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x12F0: DHW Flow Rate
// ramses_rf reference: ramses_rf/payloads/dhw.py:DhwFlowRatePayload
// ----------------------------------------------------------------------
using DhwConfigFmt = binary::Struct<"!Bh">;

std::optional<DhwConfigPayload> DhwConfigPayload::decode(const uint8_t *payload,
                                                         size_t len) {
  auto fields = DhwConfigFmt::unpack(payload, len);
  if (!fields)
    return std::nullopt;

  auto [dhw_idx, raw_flow_rate] = *fields;
  DhwConfigPayload res;
  res.dhw_index = dhw_idx;
  res.flow_rate = static_cast<float>(raw_flow_rate) / 100.0f;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1FC9: Device Binding & Remote Pairing Handshake
// ramses_rf reference: ramses_rf/binding_fsm.py
// ----------------------------------------------------------------------
using BindingTuple = binary::Struct<"!BH">;

std::optional<BindingPayload> BindingPayload::decode(const RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  if (opcode != 0x1FC9)
    return std::nullopt;

  BindingPayload res;
  if (msg.type == RAMSES_MSG_I) {
    if (msg.len == 1) {
      res.is_confirm = true;
      return res;
    }
    res.is_offer = true;
  } else if (msg.type == RAMSES_MSG_W) {
    res.is_accept = true;
  }

  // Parse 6-byte binding tuples [oem_code (1B), opcode (2B), addr (3B)]
  std::span<const uint8_t> buf(msg.payload, msg.n_payload);
  for (size_t i = 0; i + 6 <= buf.size(); i += 6) {
    auto item_slice = buf.subspan(i, 6);
    auto [oem_code, op_code] = *BindingTuple::unpack(item_slice);
    res.bindings.push_back(BindingItem{
        .oem_code = oem_code,
        .opcode = op_code,
        .address = RamsesAddress::from_bytes(item_slice.subspan(3).data()),
    });
  }

  return res;
}

RamsesMessage BindingPayload::encode_offer(const RamsesAddress &remote_addr,
                                           HvacScheme scheme) {
  RamsesAddress bcast_addr{.dev_class = 63, .id = 262142, .is_valid = true};
  uint8_t oem = get_hvac_oem_code(scheme);

  return RamsesMessageBuilder::info()
      .from(remote_addr)
      .to(bcast_addr)
      .opcode(0x1FC9)
      .append_binding(0x00, 0x22F1, remote_addr)
      .append_binding(0x00, 0x22F3, remote_addr)
      .append_binding(oem, 0x10E0, remote_addr)
      .append_binding(0x00, 0x1FC9, remote_addr)
      .build();
}

RamsesMessage BindingPayload::encode_confirm(const RamsesAddress &remote_addr,
                                             const RamsesAddress &fan_addr) {
  return RamsesMessageBuilder::info()
      .from(remote_addr)
      .to(fan_addr)
      .opcode(0x1FC9)
      .payload_byte(0x00);
}

} // namespace ramses_esp
} // namespace esphome
