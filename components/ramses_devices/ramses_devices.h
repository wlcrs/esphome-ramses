#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "components/ramses_esp/ramses_message.h"
#include "components/ramses_esp/ramses_decoder.h"
#include <string>
#include <optional>

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

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
};

class RamsesSensor : public sensor::Sensor, public Component {
 public:
  RamsesSensor() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) { this->parent_ = parent; }
  void set_device_address(const std::string &addr) { this->device_address_ = ramses_esp::RamsesAddress::from_string(addr); }
  void set_zone_index(uint8_t zone) { this->zone_index_ = zone; }
  void set_relay_index(uint8_t relay) { this->relay_index_ = relay; }
  void set_sensor_type(RamsesSensorType type) { this->sensor_type_ = type; }

  void setup() override;
  void on_message(const ramses_esp::RamsesMessage &msg);

 protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress device_address_;
  std::optional<uint8_t> zone_index_;
  std::optional<uint8_t> relay_index_;
  RamsesSensorType sensor_type_{RamsesSensorType::ZONE_TEMPERATURE};
};

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
};

class RamsesBinarySensor : public binary_sensor::BinarySensor, public Component {
 public:
  RamsesBinarySensor() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) { this->parent_ = parent; }
  void set_device_address(const std::string &addr) { this->device_address_ = ramses_esp::RamsesAddress::from_string(addr); }
  void set_zone_index(uint8_t zone) { this->zone_index_ = zone; }
  void set_sensor_type(RamsesBinarySensorType type) { this->sensor_type_ = type; }

  void setup() override;
  void on_message(const ramses_esp::RamsesMessage &msg);

 protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress device_address_;
  std::optional<uint8_t> zone_index_;
  RamsesBinarySensorType sensor_type_{RamsesBinarySensorType::FILTER_ALARM};
};

} // namespace ramses_devices
} // namespace esphome
