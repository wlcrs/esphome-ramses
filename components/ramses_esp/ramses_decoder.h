#pragma once

#include "ramses_message.h"
#include "struct.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace esphome {
namespace ramses_esp {

// Sentinel value used by Honeywell for unavailable/invalid sensor values
// Equivalent to ramses_tx/helpers.py: hex_to_temp()
static constexpr int16_t RAMSES_TEMP_SENTINEL_INVALID = 0x7FFF;
static constexpr int16_t RAMSES_TEMP_SENTINEL_DISABLED = 0x7EFF;

// ----------------------------------------------------------------------
// Opcode 0x30C9: Temperature Telemetry
// Equivalent to: ramses_rf/payloads/heating.py:TemperaturePayload /
// parse_30c9()
// ----------------------------------------------------------------------
struct ZoneTemperatureItem {
  uint8_t zone_index{0};
  float temperature{0.0f}; // Celsius
  bool is_valid{false};
};

struct TemperaturePayload {
  std::vector<ZoneTemperatureItem> zones;

  static std::optional<TemperaturePayload> decode(const uint8_t *payload,
                                                  size_t len);
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

  static std::optional<SetpointPayload> decode(const uint8_t *payload,
                                               size_t len);
  static RamsesMessage encode_write(const RamsesAddress &src,
                                    const RamsesAddress &dst,
                                    uint8_t zone_index, float setpoint);
};

// ----------------------------------------------------------------------
// Opcode 0x1F09: Gateway synchronization heartbeat / legacy sync payload
// Climate system mode is carried by 0x2E04; see SystemModePayload below.
// ----------------------------------------------------------------------
enum class SystemMode : uint8_t {
  AUTO = 0,
  HEAT_OFF = 1,
  ECO_BOOST = 2,
  AWAY = 3,
  DAY_OFF = 4,
  DAY_OFF_ECO = 5,
  AUTO_WITH_RESET = 6,
  CUSTOM = 7,
  ECO = ECO_BOOST,
  OFF = HEAT_OFF,
  UNKNOWN = 0xFF
};

const char *system_mode_to_string(SystemMode mode);
SystemMode system_mode_from_string(const std::string &str);

// Opcode 0x2E04: System Mode
// Equivalent to: ramses_rf/commands/builders/system.py:build_set_system_mode
struct SystemModePayload {
  SystemMode mode{SystemMode::AUTO};
  uint8_t mode_raw{0};

  static std::optional<SystemModePayload> decode(const uint8_t *payload,
                                                 size_t len);
  static RamsesMessage encode_write(const RamsesAddress &src,
                                    const RamsesAddress &dst, SystemMode mode);
};

struct SystemSyncPayload {
  SystemMode mode{SystemMode::AUTO};
  uint8_t mode_raw{0};
  uint16_t remaining_raw{0}; // Remaining override minutes/seconds or timestamp

  static std::optional<SystemSyncPayload> decode(const uint8_t *payload,
                                                 size_t len);
  static RamsesMessage encode_write(const RamsesAddress &src,
                                    const RamsesAddress &dst, SystemMode mode);
};

// ----------------------------------------------------------------------
// Opcode 0x3150: Heat Demand
// Equivalent to: ramses_rf/payloads/heating.py:HeatDemandPayload
// ----------------------------------------------------------------------
struct HeatDemandPayload {
  uint8_t domain_or_zone_index{0};
  float demand_percent{0.0f}; // 0.0% to 100.0%

  static std::optional<HeatDemandPayload> decode(const uint8_t *payload,
                                                 size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x0004: Zone Name
// Equivalent to: ramses_rf/payloads/heating.py:ZoneNamePayload
// (ZoneName22BPayload)
// ----------------------------------------------------------------------
struct ZoneNamePayload {
  uint8_t zone_index{0};
  std::string name;

  static std::optional<ZoneNamePayload> decode(const uint8_t *payload,
                                               size_t len);
  static RamsesMessage encode_query(const RamsesAddress &src,
                                    const RamsesAddress &dst,
                                    uint8_t zone_index);
};

// ----------------------------------------------------------------------
// Opcode 0x0005: Zone Structure
// Equivalent to: ramses_rf/payloads/heating.py:ZoneStructurePayload
// ----------------------------------------------------------------------
struct ZoneStructurePayload {
  uint8_t zone_type{0};
  uint16_t active_zone_mask{0};
  uint8_t zone_count{0};

  static std::optional<ZoneStructurePayload> decode(const uint8_t *payload,
                                                    size_t len);
  static RamsesMessage encode_query(const RamsesAddress &src,
                                    const RamsesAddress &dst);
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

  static std::optional<ZoneRolePayload> decode(const uint8_t *payload,
                                               size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x22F1 / 0x22F3: HVAC Fan State & Control
// Equivalent to: ramses_rf/payloads/hvac.py:FanStatePayload &
// ramses_rf/models/hvac_schemas.py
// ----------------------------------------------------------------------
enum class HvacScheme {
  AUTO = 0,
  ORCON = 1,
  VASCO = 2,
  ITHO = 3,
  ZEHNDER = 4,
  HOPPER = 5
};

uint8_t get_hvac_oem_code(HvacScheme scheme);

enum class FanPresetMode { AUTO, LOW, MEDIUM, HIGH, BOOST, AWAY, OFF, UNKNOWN };

const char *fan_preset_to_string(FanPresetMode mode);

struct FanStatePayload {
  uint8_t raw_mode{0};
  FanPresetMode preset_mode{FanPresetMode::UNKNOWN};
  uint8_t speed_percent{0}; // 0 - 100%

  static std::optional<FanStatePayload>
  decode(const uint8_t *payload, size_t len,
         HvacScheme scheme = HvacScheme::ORCON);
  static RamsesMessage encode_write(const RamsesAddress &src,
                                    const RamsesAddress &dst,
                                    FanPresetMode mode,
                                    HvacScheme scheme = HvacScheme::ORCON);
};

// Opcode 0x22F3: HVAC boost/timer control
// Equivalent to: ramses_rf/payloads/hvac.py:HvacVentilationControlPayload
struct FanBoostPayload {
  uint8_t header{0};
  uint8_t flags{0};
  uint16_t minutes{0};

  static std::optional<FanBoostPayload> decode(const uint8_t *payload,
                                               size_t len);
  static RamsesMessage encode_write(const RamsesAddress &src,
                                    const RamsesAddress &dst, uint16_t minutes);
};

// ----------------------------------------------------------------------
// Opcode 0x31D9: HVAC Fan Info, Bypass Damper & Mode Status
// Equivalent to: ramses_rf/payloads/hvac.py:HvacBypassStatePayload
// ----------------------------------------------------------------------
struct HvacFanInfoPayload {
  uint8_t hvac_id{0};
  uint8_t flags{0};
  uint8_t raw_fan_mode{0};
  FanPresetMode preset_mode{FanPresetMode::UNKNOWN};
  std::optional<float> fan_speed_percent; // 0.0 - 100.0%
  bool passive{false};
  bool damper_only{false};
  bool frost_cycle{false};
  bool filter_dirty{false};
  bool has_fault{false};
  std::optional<float> bypass_position;

  static std::optional<HvacFanInfoPayload>
  decode(const uint8_t *payload, size_t len,
         HvacScheme scheme = HvacScheme::AUTO);
};

// ----------------------------------------------------------------------
// Opcode 0x31DA: Comprehensive HVAC Status & Telemetry
// Equivalent to: ramses_rf/payloads/hvac.py:HvacTelemetryPayload
// ----------------------------------------------------------------------
struct HvacTelemetryPayload {
  uint8_t hvac_id{0};
  std::optional<float> exhaust_temp;
  std::optional<float> supply_temp;
  std::optional<float> indoor_temp;
  std::optional<float> outdoor_temp;
  std::optional<float> indoor_humidity;
  std::optional<float> outdoor_humidity;
  std::optional<uint16_t> co2_ppm;
  std::optional<float> bypass_position;
  std::optional<float> supply_fan_speed;
  std::optional<float> exhaust_fan_speed;
  std::optional<uint16_t> remaining_mins;

  static std::optional<HvacTelemetryPayload> decode(const uint8_t *payload,
                                                    size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x10A0 / 0x22E5: Ventilation Info & Bypass Damper

// Equivalent to: ramses_rf/payloads/hvac.py:VentilationPayload
// ----------------------------------------------------------------------
struct VentilationInfoPayload {
  uint8_t bypass_position{0}; // 0 - 100%
  bool bypass_active{false};
  bool filter_dirty{false};

  static std::optional<VentilationInfoPayload> decode(const uint8_t *payload,
                                                      size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12A0: Multi-Sensor Array (Air Quality, Humidity, Temps)
// Equivalent to: ramses_rf/payloads/hvac.py:AirQualityPayload &
// ramses_rf/quirks.py
// ----------------------------------------------------------------------
struct AirQualityPayload {
  uint8_t sensor_index{0}; // 00=indoor, 01=supply, 02=outdoor
  std::optional<float> temperature;
  std::optional<float> humidity;

  static std::optional<AirQualityPayload> decode(const uint8_t *payload,
                                                 size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x10D0: Filter Life Remaining
// Equivalent to: ramses_rf/payloads/hvac.py:FilterInfoPayload
// ----------------------------------------------------------------------
struct FilterInfoPayload {
  uint16_t remaining_days{0};
  uint16_t lifetime_days{0};
  float remaining_percent{0.0f};

  static std::optional<FilterInfoPayload> decode(const uint8_t *payload,
                                                 size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1060: Device Battery & State
// Equivalent to: ramses_rf/payloads/helpers.py
// ----------------------------------------------------------------------
struct DeviceBatteryPayload {
  uint8_t domain_or_device{0};
  uint8_t battery_percent{0}; // 0 - 100
  bool battery_low{false};

  static std::optional<DeviceBatteryPayload> decode(const uint8_t *payload,
                                                    size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12B0: Window / Door Contact State
// Equivalent to: ramses_rf/payloads/hvac.py:WindowStatePayload
// ----------------------------------------------------------------------
struct WindowStatePayload {
  uint8_t zone_index{0};
  bool window_open{false};

  static std::optional<WindowStatePayload> decode(const uint8_t *payload,
                                                  size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x22C9 / 0x2209: UFH / Setpoint Bounds
// Equivalent to: ramses_rf/payloads/hvac.py:SetpointBoundsPayload
// ----------------------------------------------------------------------
struct UfhSetpointBoundsPayload {
  uint8_t ufh_index{0};
  std::optional<float> min_temp;
  std::optional<float> max_temp;
  uint8_t mode_code{0};

  static std::optional<UfhSetpointBoundsPayload> decode(const uint8_t *payload,
                                                        size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x3EF0 / 0x3EF1 / 0x3B00: Actuator Modulation & Sync State
// Equivalent to: ramses_rf/payloads/heating.py:ActuatorStatePayload
// ----------------------------------------------------------------------
struct ActuatorStatePayload {
  uint8_t domain_id{0};
  float modulation_level{0.0f};   // 0.0 - 1.0
  float modulation_percent{0.0f}; // 0.0 - 100.0%
  bool relay_active{false};

  static std::optional<ActuatorStatePayload>
  decode(const uint8_t *payload, size_t len, uint16_t opcode = 0x3EF0);
};

// ----------------------------------------------------------------------
// Opcode 0x0418 / 0x042F / 0x0009 / 0x4401: System Fault Log
// Equivalent to: ramses_rf/payloads/system.py:SystemFaultLogPayload
// ----------------------------------------------------------------------
struct SystemFaultLogPayload {
  uint8_t log_index{0};
  uint8_t fault_code{0};
  uint8_t domain_id{0};
  bool is_fault{false};

  static std::optional<SystemFaultLogPayload>
  decode(const uint8_t *payload, size_t len, uint16_t opcode = 0x0418);
};

// ----------------------------------------------------------------------
// Opcode 0x4E01 / 0x4E02: Spider / Autotemp Dutch Smart Thermostat
// Equivalent to: ramses_rf/payloads/hvac.py:HvacSpiderTemperaturesPayload
// ----------------------------------------------------------------------
struct SpiderTemperaturesPayload {
  uint8_t hdr{0};
  std::vector<float> temperatures;
  std::optional<float> primary_temp;

  static std::optional<SpiderTemperaturesPayload> decode(const uint8_t *payload,
                                                         size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x10E0: Device Info & OEM Signature
// Equivalent to: ramses_rf/protocol/fingerprints.py & binding_fsm.py
// ----------------------------------------------------------------------
struct DeviceInfoPayload {
  uint8_t info_type{0};
  uint8_t oem_code{0}; // 0x67=Orcon, 0x08=Itho, 0x13=Vasco, 0x02=Zehnder

  static std::optional<DeviceInfoPayload> decode(const uint8_t *payload,
                                                 size_t len);
  static RamsesMessage encode_query(const RamsesAddress &src,
                                    const RamsesAddress &dst);
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

  static std::optional<OpenThermPayload> decode(const uint8_t *payload,
                                                size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1260 / 0x1F41: DHW State & Setpoints
// Equivalent to: ramses_rf/payloads/dhw.py:DhwStatePayload & DhwTempPayload
// ----------------------------------------------------------------------
struct DhwStatePayload {
  float current_temp{0.0f};
  bool current_temp_valid{false};
  float target_setpoint{0.0f};
  bool relay_active{false};
  bool dhw_enabled{true};

  static std::optional<DhwStatePayload> decode_temp(const uint8_t *payload,
                                                    size_t len);
  static std::optional<DhwStatePayload> decode_state(const uint8_t *payload,
                                                     size_t len);
  static RamsesMessage encode_write_setpoint(const RamsesAddress &src,
                                             const RamsesAddress &dst,
                                             float setpoint);
  enum class OperationMode : uint8_t {
    FOLLOW_SCHEDULE = 0,
    PERMANENT_OFF = 1,
    PERMANENT_ON = 2,
    TEMPORARY_ON = 3,
  };

  static RamsesMessage encode_write_mode(const RamsesAddress &src,
                                         const RamsesAddress &dst,
                                         OperationMode mode);
};

// ----------------------------------------------------------------------
// Opcode 0x12C0: Outdoor Temperature
// Equivalent to: ramses_rf/payloads/heating.py:OutdoorTempPayload
// ----------------------------------------------------------------------
struct OutdoorTemperaturePayload {
  float temperature{0.0f};
  bool is_valid{false};

  static std::optional<OutdoorTemperaturePayload> decode(const uint8_t *payload,
                                                         size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1298: CO2 Sensor Telemetry
// Equivalent to: ramses_rf/payloads/hvac.py:Co2Payload
// ----------------------------------------------------------------------
struct Co2SensorPayload {
  uint16_t co2_ppm{0};
  bool is_valid{false};

  static std::optional<Co2SensorPayload> decode(const uint8_t *payload,
                                                size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x0008: Relay / Actuator Demand
// Equivalent to: ramses_rf/payloads/heating.py:RelayDemandPayload
// ----------------------------------------------------------------------
struct RelayDemandPayload {
  uint8_t relay_index{0};
  float demand_percent{0.0f}; // 0.0 - 100.0%
  bool is_active{false};

  static std::optional<RelayDemandPayload> decode(const uint8_t *payload,
                                                  size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12B0: Window / Door Contact Sensor
// Equivalent to: ramses_rf/payloads/heating.py:WindowStatePayload
// ----------------------------------------------------------------------
struct ContactSensorPayload {
  uint8_t zone_index{0};
  bool is_open{false};

  static std::optional<ContactSensorPayload> decode(const uint8_t *payload,
                                                    size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x12F0: DHW Flow Rate
// Equivalent to: ramses_rf/payloads/dhw.py:DhwFlowRatePayload
// ----------------------------------------------------------------------
struct DhwConfigPayload {
  uint8_t dhw_index{0};
  float flow_rate{0.0f}; // Litres per minute

  static std::optional<DhwConfigPayload> decode(const uint8_t *payload,
                                                size_t len);
};

// ----------------------------------------------------------------------
// Opcode 0x1FC9: Device Binding & Remote Pairing Handshake
// Equivalent to: ramses_rf/binding_fsm.py
// ----------------------------------------------------------------------
struct BindingItem {
  uint8_t oem_code{0};
  uint16_t opcode{0};
  RamsesAddress address;
};

struct BindingPayload {
  std::vector<BindingItem> bindings;
  bool is_offer{false};
  bool is_accept{false};
  bool is_confirm{false};

  static std::optional<BindingPayload> decode(const RamsesMessage &msg);
  static RamsesMessage encode_offer(const RamsesAddress &remote_addr,
                                    HvacScheme scheme);
  static RamsesMessage encode_confirm(const RamsesAddress &remote_addr,
                                      const RamsesAddress &fan_addr);
};

} // namespace ramses_esp
} // namespace esphome
