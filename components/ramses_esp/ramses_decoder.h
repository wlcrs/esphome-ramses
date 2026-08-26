#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>
#include "ramses_message.h"

namespace esphome {
namespace ramses_esp {

// Sentinel value used by Honeywell for unavailable/invalid sensor values
// Equivalent to ramses_tx/helpers.py: hex_to_temp()
static constexpr int16_t RAMSES_TEMP_SENTINEL_INVALID = 0x7FFF;
static constexpr int16_t RAMSES_TEMP_SENTINEL_DISABLED = 0x7EFF;

// ----------------------------------------------------------------------
// Opcode 0x30C9: Temperature Telemetry
// Equivalent to: ramses_rf/payloads/heating.py:TemperaturePayload / parse_30c9()
// ----------------------------------------------------------------------
struct ZoneTemperatureItem {
  uint8_t zone_index{0};
  float temperature{0.0f}; // Celsius
  bool is_valid{false};
};

struct TemperaturePayload {
  std::vector<ZoneTemperatureItem> zones;

  static std::optional<TemperaturePayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x2309: Zone Setpoints
// Equivalent to: ramses_rf/payloads/heating.py:SetpointPayload
// ----------------------------------------------------------------------
struct ZoneSetpointItem {
  uint8_t zone_index{0};
  float setpoint{0.0f}; // Celsius
  bool is_valid{false};
};

struct SetpointPayload {
  std::vector<ZoneSetpointItem> zones;

  static std::optional<SetpointPayload> decode(const uint8_t *payload, size_t len);
  static RamsesMessage encode_write(const RamsesAddress &src, const RamsesAddress &dst, uint8_t zone_index, float setpoint);
};

// ----------------------------------------------------------------------
// Opcode 0x1F09: System Sync & Mode
// Equivalent to: ramses_rf/payloads/system.py:SystemSyncPayload
// ----------------------------------------------------------------------
enum class SystemMode : uint8_t {
  AUTO = 0,
  AWAY = 1,
  DAY_OFF = 2,
  CUSTOM = 3,
  ECO = 4,
  OFF = 5,
  UNKNOWN = 0xFF
};

const char *system_mode_to_string(SystemMode mode);
SystemMode system_mode_from_string(const std::string &str);

struct SystemSyncPayload {
  SystemMode mode{SystemMode::AUTO};
  uint8_t mode_raw{0};
  uint16_t remaining_raw{0}; // Remaining override minutes/seconds or timestamp

  static std::optional<SystemSyncPayload> decode(const uint8_t *payload, size_t len);
  static RamsesMessage encode_write(const RamsesAddress &src, const RamsesAddress &dst, SystemMode mode);
};

// ----------------------------------------------------------------------
// Opcode 0x3150: Heat Demand
// Equivalent to: ramses_rf/payloads/heating.py:HeatDemandPayload
// ----------------------------------------------------------------------
struct HeatDemandPayload {
  uint8_t domain_or_zone_index{0};
  float demand_percent{0.0f}; // 0.0% to 100.0%

  static std::optional<HeatDemandPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x0004: Zone Name
// Equivalent to: ramses_rf/payloads/heating.py:ZoneNamePayload (ZoneName22BPayload)
// ----------------------------------------------------------------------
struct ZoneNamePayload {
  uint8_t zone_index{0};
  std::string name;

  static std::optional<ZoneNamePayload> decode(const uint8_t *payload, size_t len);
  static RamsesMessage encode_query(const RamsesAddress &src, const RamsesAddress &dst, uint8_t zone_index);
};

// ----------------------------------------------------------------------
// Opcode 0x0005: Zone Structure
// Equivalent to: ramses_rf/payloads/heating.py:ZoneStructurePayload
// ----------------------------------------------------------------------
struct ZoneStructurePayload {
  uint8_t zone_type{0};
  uint16_t active_zone_mask{0};
  uint8_t zone_count{0};

  static std::optional<ZoneStructurePayload> decode(const uint8_t *payload, size_t len);
  static RamsesMessage encode_query(const RamsesAddress &src, const RamsesAddress &dst);
};

// ----------------------------------------------------------------------
// Opcode 0x000C: Zone Role Bindings
// Equivalent to: ramses_rf/payloads/heating.py:ZoneRolePayload
// ----------------------------------------------------------------------
struct ZoneRoleBindingItem {
  uint8_t zone_index{0};
  uint8_t role{0};
  RamsesAddress device_address;
};

struct ZoneRolePayload {
  std::vector<ZoneRoleBindingItem> bindings;

  static std::optional<ZoneRolePayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x22F1 / 0x22F3: HVAC Fan State & Control
// Equivalent to: ramses_rf/payloads/hvac.py:FanStatePayload & ramses_rf/models/hvac_schemas.py
// ----------------------------------------------------------------------
enum class HvacScheme {
  AUTO = 0,
  ORCON = 1,
  VASCO = 2,
  ITHO = 3,
  ZEHNDER = 4
};

enum class FanPresetMode {
  AUTO,
  LOW,
  MEDIUM,
  HIGH,
  BOOST,
  AWAY,
  OFF,
  UNKNOWN
};

const char *fan_preset_to_string(FanPresetMode mode);

struct FanStatePayload {
  uint8_t raw_mode{0};
  FanPresetMode preset_mode{FanPresetMode::UNKNOWN};
  uint8_t speed_percent{0}; // 0 - 100%

  static std::optional<FanStatePayload> decode(const uint8_t *payload, size_t len, HvacScheme scheme = HvacScheme::ORCON);
  static RamsesMessage encode_write(const RamsesAddress &src, const RamsesAddress &dst, FanPresetMode mode, HvacScheme scheme = HvacScheme::ORCON);
};

// ----------------------------------------------------------------------
// Opcode 0x10A0 / 0x22E5: Ventilation Info & Bypass Damper
// Equivalent to: ramses_rf/payloads/hvac.py:VentilationPayload
// ----------------------------------------------------------------------
struct VentilationInfoPayload {
  uint8_t bypass_position{0}; // 0 - 100%
  bool bypass_active{false};
  bool filter_dirty{false};

  static std::optional<VentilationInfoPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12A0: Multi-Sensor Array (Air Quality, Humidity, Temps)
// Equivalent to: ramses_rf/payloads/hvac.py:AirQualityPayload & ramses_rf/quirks.py
// ----------------------------------------------------------------------
struct AirQualityPayload {
  uint8_t sensor_index{0}; // 00=indoor, 01=supply, 02=outdoor
  std::optional<float> temperature;
  std::optional<float> humidity;

  static std::optional<AirQualityPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x10D0: Filter Life Remaining
// Equivalent to: ramses_rf/payloads/hvac.py:FilterInfoPayload
// ----------------------------------------------------------------------
struct FilterInfoPayload {
  uint16_t remaining_days{0};
  uint16_t lifetime_days{0};
  float remaining_percent{0.0f};

  static std::optional<FilterInfoPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1060: Device Battery & State
// Equivalent to: ramses_rf/payloads/helpers.py
// ----------------------------------------------------------------------
struct DeviceBatteryPayload {
  uint8_t domain_or_device{0};
  uint8_t battery_percent{0}; // 0 - 100
  bool battery_low{false};

  static std::optional<DeviceBatteryPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x10E0: Device Info & OEM Signature
// Equivalent to: ramses_rf/protocol/fingerprints.py & binding_fsm.py
// ----------------------------------------------------------------------
struct DeviceInfoPayload {
  uint8_t info_type{0};
  uint8_t oem_code{0}; // 0x67=Orcon, 0x08=Itho, 0x13=Vasco, 0x02=Zehnder

  static std::optional<DeviceInfoPayload> decode(const uint8_t *payload, size_t len);
  static RamsesMessage encode_query(const RamsesAddress &src, const RamsesAddress &dst);
};

// ----------------------------------------------------------------------
// Opcode 0x3220: OpenTherm Telemetry
// Equivalent to: ramses_rf/payloads/opentherm.py:OpenthermPayload
// ----------------------------------------------------------------------
struct OpenThermPayload {
  uint8_t msg_id{0};
  bool flame_active{false};
  bool fault_active{false};
  float modulation_percent{0.0f}; // 0.0 - 100.0%
  std::optional<float> flow_temp;
  std::optional<float> return_temp;

  static std::optional<OpenThermPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1260 / 0x1F41: DHW State & Setpoints
// Equivalent to: ramses_rf/payloads/dhw.py:DhwStatePayload & DhwTempPayload
// ----------------------------------------------------------------------
struct DhwStatePayload {
  float current_temp{0.0f};
  float target_setpoint{0.0f};
  bool relay_active{false};
  bool dhw_enabled{true};

  static std::optional<DhwStatePayload> decode_temp(const uint8_t *payload, size_t len);
  static std::optional<DhwStatePayload> decode_state(const uint8_t *payload, size_t len);
  static RamsesMessage encode_write_setpoint(const RamsesAddress &src, const RamsesAddress &dst, float setpoint);
};

// ----------------------------------------------------------------------
// Opcode 0x12C0: Outdoor Temperature
// Equivalent to: ramses_rf/payloads/heating.py:OutdoorTempPayload
// ----------------------------------------------------------------------
struct OutdoorTemperaturePayload {
  float temperature{0.0f};
  bool is_valid{false};

  static std::optional<OutdoorTemperaturePayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1298: CO2 Sensor Telemetry
// Equivalent to: ramses_rf/payloads/hvac.py:Co2Payload
// ----------------------------------------------------------------------
struct Co2SensorPayload {
  uint16_t co2_ppm{0};
  bool is_valid{false};

  static std::optional<Co2SensorPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x0008: Relay / Actuator Demand
// Equivalent to: ramses_rf/payloads/heating.py:RelayDemandPayload
// ----------------------------------------------------------------------
struct RelayDemandPayload {
  uint8_t relay_index{0};
  float demand_percent{0.0f}; // 0.0 - 100.0%
  bool is_active{false};

  static std::optional<RelayDemandPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12B0: Window / Door Contact Sensor
// Equivalent to: ramses_rf/payloads/heating.py:WindowStatePayload
// ----------------------------------------------------------------------
struct ContactSensorPayload {
  uint8_t zone_index{0};
  bool is_open{false};

  static std::optional<ContactSensorPayload> decode(const uint8_t *payload, size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12F0: DHW Configuration / Setpoint
// Equivalent to: ramses_rf/payloads/dhw.py:DhwConfigPayload
// ----------------------------------------------------------------------
struct DhwConfigPayload {
  uint8_t dhw_index{0};
  float setpoint_temperature{0.0f};

  static std::optional<DhwConfigPayload> decode(const uint8_t *payload, size_t len);
};

} // namespace ramses_esp
} // namespace esphome
