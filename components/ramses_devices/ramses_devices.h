#pragma once

#include "components/ramses_devices/ramses_entity.h"
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include <optional>
#include <string>

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace ramses_devices {

// ----------------------------------------------------------------------
// Base Sensor Types for RAMSES Devices
// ----------------------------------------------------------------------

enum class RamsesSensorType {
  ZONE_TEMPERATURE,
  ZONE_SETPOINT,
  OUTDOOR_TEMPERATURE,
  HEAT_DEMAND,
  RELAY_DEMAND,
  CO2,
  INDOOR_HUMIDITY,
  OUTDOOR_HUMIDITY,
  AIR_QUALITY_TEMPERATURE,
  BYPASS_POSITION,
  FILTER_REMAINING_DAYS,
  FILTER_LIFETIME_DAYS,
  FILTER_REMAINING_PERCENT,
  OPENTHERM_MODULATION,
  OPENTHERM_FLOW_TEMP,
  OPENTHERM_RETURN_TEMP,
  BATTERY_LEVEL,
  SUPPLY_TEMPERATURE,
  EXHAUST_TEMPERATURE,
  SUPPLY_FAN_SPEED,
  EXHAUST_FAN_SPEED,
  REMAINING_MINS,
  ACTUATOR_MODULATION,
  UFH_MIN_TEMP,
  UFH_MAX_TEMP,
  SPIDER_TEMPERATURE,
  FAULT_CODE,
};

#ifdef USE_SENSOR
class RamsesSensor : public sensor::Sensor,
                     public Component,
                     public RamsesEntityBase {
public:
  RamsesSensor() = default;

  void set_zone_index(uint8_t zone) { this->zone_index_ = zone; }
  void set_relay_index(uint8_t relay) { this->relay_index_ = relay; }
  void set_sensor_type(RamsesSensorType type) { this->sensor_type_ = type; }

  void setup() override;
  void publish_state(float state);

protected:
  void handle_message(const ramses_esp::RamsesMessage &msg) override;

  std::optional<uint8_t> zone_index_;
  std::optional<uint8_t> relay_index_;
  RamsesSensorType sensor_type_{RamsesSensorType::ZONE_TEMPERATURE};
};
#endif

// ----------------------------------------------------------------------
// Base Binary Sensor Types for RAMSES Devices
// ----------------------------------------------------------------------

enum class RamsesBinarySensorType {
  FILTER_ALARM,
  FLAME_ACTIVE,
  FAULT_ALARM,
  WINDOW_OPEN,
  BYPASS_ACTIVE,
  BATTERY_LOW,
  ACTUATOR_RELAY,
};

#ifdef USE_BINARY_SENSOR
class RamsesBinarySensor : public binary_sensor::BinarySensor,
                           public Component,
                           public RamsesEntityBase {
public:
  RamsesBinarySensor() = default;

  void set_zone_index(uint8_t zone) { this->zone_index_ = zone; }
  void set_sensor_type(RamsesBinarySensorType type) {
    this->sensor_type_ = type;
  }

  void setup() override;

protected:
  void handle_message(const ramses_esp::RamsesMessage &msg) override;

  std::optional<uint8_t> zone_index_;
  RamsesBinarySensorType sensor_type_{RamsesBinarySensorType::FILTER_ALARM};
};
#endif

} // namespace ramses_devices
} // namespace esphome
