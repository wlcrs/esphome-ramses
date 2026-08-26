#include "ramses_decoder.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace esphome {
namespace ramses_esp {

// ----------------------------------------------------------------------
// Helper: Convert signed fixed-point (0.01 C) to float
// Equivalent to ramses_tx/helpers.py: hex_to_temp()
// ----------------------------------------------------------------------
static inline bool parse_temperature_raw(int16_t raw, float &out_temp) {
  if (raw == RAMSES_TEMP_SENTINEL_INVALID || raw == RAMSES_TEMP_SENTINEL_DISABLED) {
    return false;
  }
  out_temp = static_cast<float>(raw) / 100.0f;
  return true;
}

// ----------------------------------------------------------------------
// Opcode 0x30C9: Temperature Telemetry
// ramses_rf reference: ramses_rf/payloads/heating.py:TemperaturePayload / parse_30c9()
// ----------------------------------------------------------------------
std::optional<TemperaturePayload> TemperaturePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  TemperaturePayload res;
  if (len == 2) {
    ZoneTemperatureItem item;
    item.zone_index = 0;
    int16_t raw_temp = static_cast<int16_t>((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);
    item.is_valid = parse_temperature_raw(raw_temp, item.temperature);
    res.zones.push_back(item);
    return res;
  }

  // Multi-zone arrays: each zone item is 3 bytes: [zone_index (1B), temp_hi (1B), temp_lo (1B)]
  for (size_t i = 0; i + 3 <= len; i += 3) {
    ZoneTemperatureItem item;
    item.zone_index = payload[i];
    int16_t raw_temp = static_cast<int16_t>((static_cast<uint16_t>(payload[i + 1]) << 8) | payload[i + 2]);
    item.is_valid = parse_temperature_raw(raw_temp, item.temperature);
    res.zones.push_back(item);
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x2309: Zone Setpoints
// ramses_rf reference: ramses_rf/payloads/heating.py:SetpointPayload
// ----------------------------------------------------------------------
std::optional<SetpointPayload> SetpointPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  SetpointPayload res;
  for (size_t i = 0; i + 3 <= len; i += 3) {
    ZoneSetpointItem item;
    item.zone_index = payload[i];
    int16_t raw_sp = static_cast<int16_t>((static_cast<uint16_t>(payload[i + 1]) << 8) | payload[i + 2]);
    item.is_valid = parse_temperature_raw(raw_sp, item.setpoint);
    res.zones.push_back(item);
  }
  return res;
}

RamsesMessage SetpointPayload::encode_write(const RamsesAddress &src, const RamsesAddress &dst, uint8_t zone_index, float setpoint) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_W;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x23;
  msg.opcode[1] = 0x09;
  msg.len = msg.n_payload = 3;
  msg.payload[0] = zone_index;
  int16_t raw_sp = static_cast<int16_t>(std::round(setpoint * 100.0f));
  msg.payload[1] = (raw_sp >> 8) & 0xFF;
  msg.payload[2] = raw_sp & 0xFF;
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x1F09: System Sync & Mode
// ramses_rf reference: ramses_rf/payloads/system.py:SystemSyncPayload
// ----------------------------------------------------------------------
const char *system_mode_to_string(SystemMode mode) {
  switch (mode) {
    case SystemMode::AUTO: return "auto";
    case SystemMode::HEAT_OFF: return "heat_off";
    case SystemMode::ECO_BOOST: return "eco_boost";
    case SystemMode::AWAY: return "away";
    case SystemMode::DAY_OFF: return "day_off";
    case SystemMode::DAY_OFF_ECO: return "day_off_eco";
    case SystemMode::AUTO_WITH_RESET: return "auto_with_reset";
    case SystemMode::CUSTOM: return "custom";
    default: return "unknown";
  }
}

SystemMode system_mode_from_string(const std::string &str) {
  if (str == "auto") return SystemMode::AUTO;
  if (str == "heat_off" || str == "off") return SystemMode::HEAT_OFF;
  if (str == "eco_boost" || str == "eco") return SystemMode::ECO_BOOST;
  if (str == "away") return SystemMode::AWAY;
  if (str == "day_off") return SystemMode::DAY_OFF;
  if (str == "day_off_eco") return SystemMode::DAY_OFF_ECO;
  if (str == "auto_with_reset") return SystemMode::AUTO_WITH_RESET;
  if (str == "custom") return SystemMode::CUSTOM;
  return SystemMode::UNKNOWN;
}

std::optional<SystemModePayload> SystemModePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 1) return std::nullopt;

  SystemModePayload res;
  res.mode_raw = payload[0];
  res.mode = SystemMode::UNKNOWN;
  switch (res.mode_raw) {
    case 0: res.mode = SystemMode::AUTO; break;
    case 1: res.mode = SystemMode::HEAT_OFF; break;
    case 2: res.mode = SystemMode::ECO_BOOST; break;
    case 3: res.mode = SystemMode::AWAY; break;
    case 4: res.mode = SystemMode::DAY_OFF; break;
    case 5: res.mode = SystemMode::DAY_OFF_ECO; break;
    case 6: res.mode = SystemMode::AUTO_WITH_RESET; break;
    default: break;
  }
  return res;
}

RamsesMessage SystemModePayload::encode_write(const RamsesAddress &src, const RamsesAddress &dst, SystemMode mode) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_W;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x2E;
  msg.opcode[1] = 0x04;
  msg.len = msg.n_payload = 8;
  msg.payload[0] = static_cast<uint8_t>(mode);
  return msg;
}

std::optional<SystemSyncPayload> SystemSyncPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 1) return std::nullopt;

  SystemSyncPayload res;
  res.mode_raw = payload[0];
  switch (res.mode_raw) {
    case 0: res.mode = SystemMode::AUTO; break;
    case 1: res.mode = SystemMode::AWAY; break;
    case 2: res.mode = SystemMode::DAY_OFF; break;
    case 3: res.mode = SystemMode::CUSTOM; break;
    case 4: res.mode = SystemMode::ECO; break;
    case 5: res.mode = SystemMode::OFF; break;
    default: res.mode = SystemMode::UNKNOWN; break;
  }

  if (len >= 3) {
    res.remaining_raw = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
  }
  return res;
}

RamsesMessage SystemSyncPayload::encode_write(const RamsesAddress &src, const RamsesAddress &dst, SystemMode mode) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_W;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x1F;
  msg.opcode[1] = 0x09;
  msg.len = msg.n_payload = 1;
  msg.payload[0] = static_cast<uint8_t>(mode);
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x3150: Heat Demand
// ramses_rf reference: ramses_rf/payloads/heating.py:HeatDemandPayload
// ----------------------------------------------------------------------
std::optional<HeatDemandPayload> HeatDemandPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  HeatDemandPayload res;
  res.domain_or_zone_index = payload[0];
  // Value 0..200 maps to 0.0%..100.0%
  res.demand_percent = (static_cast<float>(payload[1]) / 200.0f) * 100.0f;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x0004: Zone Name
// ramses_rf reference: ramses_rf/payloads/heating.py:ZoneNamePayload (ZoneName22BPayload)
// ----------------------------------------------------------------------
std::optional<ZoneNamePayload> ZoneNamePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  ZoneNamePayload res;
  res.zone_index = payload[0];
  // 22B layout: [index (1B), flag (1B), ASCII chars up to 20B]
  size_t name_len = (len >= 22) ? 20 : (len - 2);
  const char *raw_chars = reinterpret_cast<const char *>(&payload[2]);
  
  // Find first null or non-printable character
  size_t actual_len = 0;
  while (actual_len < name_len && raw_chars[actual_len] != '\0' && raw_chars[actual_len] >= 32 && raw_chars[actual_len] <= 126) {
    actual_len++;
  }
  res.name = std::string(raw_chars, actual_len);
  return res;
}

RamsesMessage ZoneNamePayload::encode_query(const RamsesAddress &src, const RamsesAddress &dst, uint8_t zone_index) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_RQ;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x00;
  msg.opcode[1] = 0x04;
  msg.len = msg.n_payload = 1;
  msg.payload[0] = zone_index;
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x0005: Zone Structure
// ramses_rf reference: ramses_rf/payloads/heating.py:ZoneStructurePayload
// ----------------------------------------------------------------------
std::optional<ZoneStructurePayload> ZoneStructurePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  ZoneStructurePayload res;
  res.zone_type = payload[0];
  res.active_zone_mask = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
  if (len >= 4) {
    res.zone_count = payload[3];
  } else {
    // Count active bits in mask
    res.zone_count = 0;
    for (int b = 0; b < 16; b++) {
      if (res.active_zone_mask & (1 << b)) res.zone_count++;
    }
  }
  return res;
}

RamsesMessage ZoneStructurePayload::encode_query(const RamsesAddress &src, const RamsesAddress &dst) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_RQ;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x00;
  msg.opcode[1] = 0x05;
  msg.len = msg.n_payload = 1;
  msg.payload[0] = 0x00;
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x000C: Zone Role Bindings
// ramses_rf reference: ramses_rf/payloads/heating.py:ZoneRolePayload
// ----------------------------------------------------------------------
std::optional<ZoneRolePayload> ZoneRolePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 5) return std::nullopt;

  ZoneRolePayload res;
  // Each entry is 5 bytes: [zone_idx (1B), role (1B), addr_bytes (3B)]
  for (size_t i = 0; i + 5 <= len; i += 5) {
    ZoneRoleBindingItem item;
    item.zone_index = payload[i];
    item.role = payload[i + 1];
    item.device_address = RamsesAddress::from_bytes(&payload[i + 2]);
    res.bindings.push_back(item);
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x22F1 / 0x22F3: HVAC Fan State & Control
// ramses_rf reference: ramses_rf/payloads/hvac.py:FanStatePayload & ramses_rf/models/hvac_schemas.py
// ----------------------------------------------------------------------
const char *fan_preset_to_string(FanPresetMode mode) {
  switch (mode) {
    case FanPresetMode::AUTO: return "auto";
    case FanPresetMode::LOW: return "low";
    case FanPresetMode::MEDIUM: return "medium";
    case FanPresetMode::HIGH: return "high";
    case FanPresetMode::BOOST: return "boost";
    case FanPresetMode::AWAY: return "away";
    case FanPresetMode::OFF: return "off";
    default: return "unknown";
  }
}

std::optional<FanStatePayload> FanStatePayload::decode(const uint8_t *payload, size_t len, HvacScheme scheme) {
  if (payload == nullptr || len < 2) return std::nullopt;

  FanStatePayload res;
  res.raw_mode = payload[1];

  switch (scheme) {
    case HvacScheme::ORCON:
    default:
      // Orcon: 00=away, 01=low, 02=medium, 03=high, 04/05=auto, 06=boost, 07=off.
      switch (res.raw_mode) {
        case 0x00: res.preset_mode = FanPresetMode::AWAY; res.speed_percent = 15; break;
        case 0x01: res.preset_mode = FanPresetMode::LOW; res.speed_percent = 33; break;
        case 0x02: res.preset_mode = FanPresetMode::MEDIUM; res.speed_percent = 66; break;
        case 0x03: res.preset_mode = FanPresetMode::HIGH; res.speed_percent = 100; break;
        case 0x04:
        case 0x05: res.preset_mode = FanPresetMode::AUTO; res.speed_percent = 50; break;
        case 0x06: res.preset_mode = FanPresetMode::BOOST; res.speed_percent = 100; break;
        case 0x07: res.preset_mode = FanPresetMode::OFF; res.speed_percent = 0; break;
        default: res.preset_mode = FanPresetMode::UNKNOWN; break;
      }
      break;

    case HvacScheme::VASCO:
    case HvacScheme::ZEHNDER:
      switch (res.raw_mode) {
        case 0x00: res.preset_mode = FanPresetMode::OFF; res.speed_percent = 0; break;
        case 0x01: res.preset_mode = FanPresetMode::AWAY; res.speed_percent = 15; break;
        case 0x02: res.preset_mode = FanPresetMode::LOW; res.speed_percent = 33; break;
        case 0x03: res.preset_mode = FanPresetMode::MEDIUM; res.speed_percent = 66; break;
        case 0x04: res.preset_mode = FanPresetMode::HIGH; res.speed_percent = 100; break;
        case 0x05: res.preset_mode = FanPresetMode::AUTO; res.speed_percent = 50; break;
        default: res.preset_mode = FanPresetMode::UNKNOWN; break;
      }
      break;

    case HvacScheme::ITHO:
      // Itho uses a mode index: 00=off, 01=trickle, 02=low, 03=medium, 04=high.
      switch (res.raw_mode) {
        case 0x00: res.preset_mode = FanPresetMode::OFF; res.speed_percent = 0; break;
        case 0x01: res.preset_mode = FanPresetMode::LOW; res.speed_percent = 20; break;
        case 0x02: res.preset_mode = FanPresetMode::LOW; res.speed_percent = 33; break;
        case 0x03: res.preset_mode = FanPresetMode::MEDIUM; res.speed_percent = 66; break;
        case 0x04: res.preset_mode = FanPresetMode::HIGH; res.speed_percent = 100; break;
        default: res.preset_mode = FanPresetMode::UNKNOWN; break;
      }
      break;
  }

  return res;
}

RamsesMessage FanStatePayload::encode_write(const RamsesAddress &src, const RamsesAddress &dst, FanPresetMode mode, HvacScheme scheme) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_W;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x22;
  msg.opcode[1] = 0xF1;
  msg.len = msg.n_payload = 3;
  msg.payload[0] = 0x00;

  uint8_t mode_byte = 0x00;
  switch (scheme) {
    case HvacScheme::ORCON:
      switch (mode) {
        case FanPresetMode::AWAY: mode_byte = 0x00; break;
        case FanPresetMode::LOW: mode_byte = 0x01; break;
        case FanPresetMode::MEDIUM: mode_byte = 0x02; break;
        case FanPresetMode::HIGH: mode_byte = 0x03; break;
        case FanPresetMode::AUTO: mode_byte = 0x04; break;
        case FanPresetMode::BOOST: mode_byte = 0x06; break;
        case FanPresetMode::OFF: mode_byte = 0x07; break;
        default: break;
      }
      break;
    case HvacScheme::ITHO:
      switch (mode) {
        case FanPresetMode::OFF: mode_byte = 0x00; break;
        case FanPresetMode::LOW: mode_byte = 0x02; break;
        case FanPresetMode::MEDIUM: mode_byte = 0x03; break;
        case FanPresetMode::HIGH:
        case FanPresetMode::BOOST: mode_byte = 0x04; break;
        default: break;
      }
      break;
    case HvacScheme::VASCO:
    case HvacScheme::ZEHNDER:
      switch (mode) {
        case FanPresetMode::OFF: mode_byte = 0x00; break;
        case FanPresetMode::AWAY: mode_byte = 0x01; break;
        case FanPresetMode::LOW: mode_byte = 0x02; break;
        case FanPresetMode::MEDIUM: mode_byte = 0x03; break;
        case FanPresetMode::HIGH:
        case FanPresetMode::BOOST: mode_byte = 0x04; break;
        case FanPresetMode::AUTO: mode_byte = 0x05; break;
        default: break;
      }
      break;
  }

  msg.payload[1] = mode_byte;
  msg.payload[2] = 0xFF;
  return msg;
}

std::optional<FanBoostPayload> FanBoostPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  FanBoostPayload res;
  res.header = payload[0];
  res.flags = payload[1];
  res.minutes = payload[2];
  if (res.flags & 0x40) res.minutes = static_cast<uint16_t>(res.minutes * 60);
  return res;
}

RamsesMessage FanBoostPayload::encode_write(const RamsesAddress &src, const RamsesAddress &dst, uint16_t minutes) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_W;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x22;
  msg.opcode[1] = 0xF3;
  msg.len = msg.n_payload = 3;
  msg.payload[0] = 0x00;
  if (minutes > 255 && minutes % 60 == 0 && minutes / 60 <= 255) {
    msg.payload[1] = 0x40;
    msg.payload[2] = static_cast<uint8_t>(minutes / 60);
  } else {
    msg.payload[1] = 0x00;
    msg.payload[2] = static_cast<uint8_t>(std::min<uint16_t>(minutes, 255));
  }
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x10A0 / 0x22E5: Ventilation Info & Bypass Damper
// ramses_rf reference: ramses_rf/payloads/hvac.py:VentilationPayload
// ----------------------------------------------------------------------
std::optional<VentilationInfoPayload> VentilationInfoPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  VentilationInfoPayload res;
  res.bypass_position = payload[0];
  res.bypass_active = (res.bypass_position > 0);
  res.filter_dirty = (payload[1] & 0x01) != 0;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x12A0: Multi-Sensor Array (Air Quality, Humidity, Temps)
// ramses_rf reference: ramses_rf/payloads/hvac.py:AirQualityPayload & ramses_rf/quirks.py
// ----------------------------------------------------------------------
std::optional<AirQualityPayload> AirQualityPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  AirQualityPayload res;
  res.sensor_index = payload[0];

  if (len == 2) {
    if (payload[1] <= 100) res.humidity = static_cast<float>(payload[1]);
    return res;
  }

  if (len < 4) return std::nullopt;
  
  // Humidity is payload[1] (0..100%)
  if (payload[1] <= 100) {
    res.humidity = static_cast<float>(payload[1]);
  }

  // Temperature is payload[2..3] fixed point 0.01 C
  int16_t raw_t = static_cast<int16_t>((static_cast<uint16_t>(payload[2]) << 8) | payload[3]);
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
std::optional<FilterInfoPayload> FilterInfoPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 4) return std::nullopt;

  FilterInfoPayload res;
  // 6-byte layout: [hdr (1B), remaining_days (1B), total_days (1B), remaining_pct_raw (1B), trailer (2B)]
  res.remaining_days = payload[1];
  res.lifetime_days = payload[2];
  res.remaining_percent = (static_cast<float>(payload[3]) / 200.0f) * 100.0f;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1060: Device Battery & State
// ramses_rf reference: ramses_rf/payloads/helpers.py
// ----------------------------------------------------------------------
std::optional<DeviceBatteryPayload> DeviceBatteryPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  DeviceBatteryPayload res;
  res.domain_or_device = payload[0];
  res.battery_percent = payload[1];
  if (len >= 3) {
    res.battery_low = (payload[2] == 0x00 || payload[1] < 20);
  }
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x10E0: Device Info & OEM Signature
// ramses_rf reference: ramses_rf/protocol/fingerprints.py & binding_fsm.py
// ----------------------------------------------------------------------
std::optional<DeviceInfoPayload> DeviceInfoPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 1) return std::nullopt;

  DeviceInfoPayload res;
  res.info_type = payload[0];
  if (len < 7) return res;
  res.oem_code = payload[6]; // Standard offset for short OEM vendor byte
  if (len >= 8 && (res.oem_code == 0 || res.oem_code == 0x0A || res.oem_code == 0xFF || payload[7] == 0x6A)) {
    res.oem_code = payload[7]; // Extended OEM vendor byte (e.g. Brofer / Hopper D375: 0x6A)
  }
  return res;
}

RamsesMessage DeviceInfoPayload::encode_query(const RamsesAddress &src, const RamsesAddress &dst) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_RQ;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x10;
  msg.opcode[1] = 0xE0;
  msg.len = msg.n_payload = 1;
  msg.payload[0] = 0x00;
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x3220: OpenTherm Telemetry
// ramses_rf reference: ramses_rf/payloads/opentherm.py:OpenthermPayload
// ----------------------------------------------------------------------
std::optional<OpenThermPayload> OpenThermPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  OpenThermPayload res;
  res.msg_id = payload[0];
  res.flame_active = (payload[1] & 0x08) != 0;
  res.fault_active = (payload[1] & 0x01) != 0;

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
      res.flow_temp = static_cast<float>(u8_hi) + (static_cast<float>(u8_lo) / 256.0f);
    }
    // Data ID 0x1C: Return Water Temperature (f8.8 float)
    else if (data_id == 0x1C) {
      res.return_temp = static_cast<float>(u8_hi) + (static_cast<float>(u8_lo) / 256.0f);
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
// ramses_rf reference: ramses_rf/payloads/dhw.py:DhwStatePayload & DhwTempPayload
// ----------------------------------------------------------------------
std::optional<DhwStatePayload> DhwStatePayload::decode_temp(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  DhwStatePayload res;
  int16_t raw_t = static_cast<int16_t>((static_cast<uint16_t>(payload[1]) << 8) | payload[2]);
  parse_temperature_raw(raw_t, res.current_temp);
  return res;
}

// Opcode 0x1F41: DHW State
// Equivalent to: ramses_rf/payloads/dhw.py:DhwState2BPayload / DhwState3BPayload
std::optional<DhwStatePayload> DhwStatePayload::decode_state(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  DhwStatePayload res;
  uint8_t active_flag = payload[1];
  res.dhw_enabled = (active_flag != 0xFF);
  res.relay_active = (active_flag == 0x01);
  return res;
}

RamsesMessage DhwStatePayload::encode_write_setpoint(const RamsesAddress &src, const RamsesAddress &dst, float setpoint) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_W;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  src.to_bytes(msg.addr[0]);
  dst.to_bytes(msg.addr[1]);
  msg.opcode[0] = 0x12;
  msg.opcode[1] = 0x60;
  msg.len = msg.n_payload = 3;
  msg.payload[0] = 0x00; // DHW zone index
  int16_t raw_sp = static_cast<int16_t>(std::round(setpoint * 100.0f));
  msg.payload[1] = (raw_sp >> 8) & 0xFF;
  msg.payload[2] = raw_sp & 0xFF;
  return msg;
}

// ----------------------------------------------------------------------
// Opcode 0x12C0: Outdoor Temperature
// ramses_rf reference: ramses_rf/payloads/heating.py:OutdoorTempPayload
// ----------------------------------------------------------------------
std::optional<OutdoorTemperaturePayload> OutdoorTemperaturePayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  OutdoorTemperaturePayload res;
  int16_t raw_t = static_cast<int16_t>((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);
  res.is_valid = parse_temperature_raw(raw_t, res.temperature);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1298: CO2 Sensor Telemetry
// ramses_rf reference: ramses_rf/payloads/hvac.py:Co2Payload
// ----------------------------------------------------------------------
std::optional<Co2SensorPayload> Co2SensorPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  Co2SensorPayload res;
  // 2-byte or 3-byte variants
  if (len == 2) {
    res.co2_ppm = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
  } else {
    res.co2_ppm = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
  }
  res.is_valid = (res.co2_ppm < 0x7FFF);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x0008: Relay / Actuator Demand
// ramses_rf reference: ramses_rf/payloads/heating.py:RelayDemandPayload
// ----------------------------------------------------------------------
std::optional<RelayDemandPayload> RelayDemandPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  RelayDemandPayload res;
  res.relay_index = payload[0];
  res.demand_percent = (static_cast<float>(payload[1]) / 200.0f) * 100.0f;
  res.is_active = (payload[1] > 0);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x12B0: Window / Door Contact Sensor
// ramses_rf reference: ramses_rf/payloads/heating.py:WindowStatePayload
// ----------------------------------------------------------------------
std::optional<ContactSensorPayload> ContactSensorPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 2) return std::nullopt;

  ContactSensorPayload res;
  res.zone_index = payload[0];
  res.is_open = (payload[1] != 0x00);
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x12F0: DHW Flow Rate
// ramses_rf reference: ramses_rf/payloads/dhw.py:DhwFlowRatePayload
// ----------------------------------------------------------------------
std::optional<DhwConfigPayload> DhwConfigPayload::decode(const uint8_t *payload, size_t len) {
  if (payload == nullptr || len < 3) return std::nullopt;

  DhwConfigPayload res;
  res.dhw_index = payload[0];
  int16_t raw_flow_rate = static_cast<int16_t>((static_cast<uint16_t>(payload[1]) << 8) | payload[2]);
  res.flow_rate = static_cast<float>(raw_flow_rate) / 100.0f;
  return res;
}

// ----------------------------------------------------------------------
// Opcode 0x1FC9: Device Binding & Remote Pairing Handshake
// ramses_rf reference: ramses_rf/binding_fsm.py
// ----------------------------------------------------------------------
std::optional<BindingPayload> BindingPayload::decode(const RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  if (opcode != 0x1FC9) return std::nullopt;

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
  for (size_t i = 0; i + 6 <= msg.n_payload; i += 6) {
    BindingItem item;
    item.oem_code = msg.payload[i];
    item.opcode = (static_cast<uint16_t>(msg.payload[i + 1]) << 8) | msg.payload[i + 2];
    item.address = RamsesAddress::from_bytes(&msg.payload[i + 3]);
    res.bindings.push_back(item);
  }

  return res;
}

RamsesMessage BindingPayload::encode_offer(const RamsesAddress &remote_addr, HvacScheme scheme) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_I;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  remote_addr.to_bytes(msg.addr[0]);

  // Target: 63:262142 (broadcast)
  RamsesAddress bcast_addr;
  bcast_addr.dev_class = 63;
  bcast_addr.id = 262142;
  bcast_addr.is_valid = true;
  bcast_addr.to_bytes(msg.addr[1]);

  msg.opcode[0] = 0x1F;
  msg.opcode[1] = 0xC9;

  uint8_t oem = 0x67; // Orcon default
  if (scheme == HvacScheme::ORCON) oem = 0x67;
  else if (scheme == HvacScheme::VASCO) oem = 0x66;
  else if (scheme == HvacScheme::ITHO) oem = 0x01;
  else if (scheme == HvacScheme::ZEHNDER) oem = 0x02;

  uint8_t remote_b[3];
  remote_addr.to_bytes(remote_b);

  uint8_t p[24] = {
    0x00, 0x22, 0xF1, remote_b[0], remote_b[1], remote_b[2],
    0x00, 0x22, 0xF3, remote_b[0], remote_b[1], remote_b[2],
    oem,  0x10, 0xE0, remote_b[0], remote_b[1], remote_b[2],
    0x00, 0x1F, 0xC9, remote_b[0], remote_b[1], remote_b[2]
  };

  msg.len = msg.n_payload = 24;
  memcpy(msg.payload, p, 24);
  return msg;
}

RamsesMessage BindingPayload::encode_confirm(const RamsesAddress &remote_addr, const RamsesAddress &fan_addr) {
  RamsesMessage msg;
  msg.type = RAMSES_MSG_I;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
  remote_addr.to_bytes(msg.addr[0]);
  fan_addr.to_bytes(msg.addr[1]);

  msg.opcode[0] = 0x1F;
  msg.opcode[1] = 0xC9;
  msg.len = msg.n_payload = 1;
  msg.payload[0] = 0x00;
  return msg;
}

} // namespace ramses_esp
} // namespace esphome
