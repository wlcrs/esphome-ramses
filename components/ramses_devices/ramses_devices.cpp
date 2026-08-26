#include "ramses_devices.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_devices";

void RamsesSensor::setup() {
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback([this](const ramses_esp::RamsesMessage &msg) {
      this->on_message(msg);
    });
  }
#endif
}

void RamsesBinarySensor::setup() {
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback([this](const ramses_esp::RamsesMessage &msg) {
      this->on_message(msg);
    });
  }
#endif
}

static inline bool address_matches(const ramses_esp::RamsesAddress &configured, const ramses_esp::RamsesMessage &msg) {
  if (!configured.is_valid) return true;
  ramses_esp::RamsesAddress src = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
  if (src == configured) return true;
  if (msg.fields & RAMSES_F_ADDR2) {
    ramses_esp::RamsesAddress targ = ramses_esp::RamsesAddress::from_bytes(msg.addr[2]);
    if (targ == configured) return true;
  }
  return false;
}

void RamsesSensor::on_message(const ramses_esp::RamsesMessage &msg) {
  if (!address_matches(this->device_address_, msg)) return;

  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  switch (this->sensor_type_) {
    case RamsesSensorType::ZONE_TEMPERATURE:
      if (opcode == 0x30C9) {
        auto dec = ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          for (const auto &item : dec->zones) {
            if (!this->zone_index_.has_value() || item.zone_index == *this->zone_index_) {
              if (item.is_valid) {
                this->publish_state(item.temperature);
              }
              break;
            }
          }
        }
      }
      break;

    case RamsesSensorType::ZONE_SETPOINT:
      if (opcode == 0x2309) {
        auto dec = ramses_esp::SetpointPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          for (const auto &item : dec->zones) {
            if (!this->zone_index_.has_value() || item.zone_index == *this->zone_index_) {
              if (item.is_valid) {
                this->publish_state(item.setpoint);
              }
              break;
            }
          }
        }
      }
      break;

    case RamsesSensorType::OUTDOOR_TEMPERATURE:
      if (opcode == 0x12C0) {
        auto dec = ramses_esp::OutdoorTemperaturePayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->is_valid) {
          this->publish_state(dec->temperature);
        }
      }
      break;

    case RamsesSensorType::HEAT_DEMAND:
      if (opcode == 0x3150) {
        auto dec = ramses_esp::HeatDemandPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          if (!this->zone_index_.has_value() || dec->domain_or_zone_index == *this->zone_index_) {
            this->publish_state(dec->demand_percent);
          }
        }
      } else if (opcode == 0x0008) {
        auto dec = ramses_esp::RelayDemandPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && (!this->relay_index_.has_value() || dec->relay_index == *this->relay_index_)) {
          this->publish_state(dec->demand_percent);
        }
      }
      break;

    case RamsesSensorType::RELAY_DEMAND:
      if (opcode == 0x0008) {
        auto dec = ramses_esp::RelayDemandPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && (!this->relay_index_.has_value() || dec->relay_index == *this->relay_index_)) {
          this->publish_state(dec->demand_percent);
        }
      }
      break;

    case RamsesSensorType::CO2:
      if (opcode == 0x1298) {
        auto dec = ramses_esp::Co2SensorPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->is_valid) {
          this->publish_state(dec->co2_ppm);
        }
      }
      break;

    case RamsesSensorType::INDOOR_HUMIDITY:
      if (opcode == 0x12A0) {
        auto dec = ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->sensor_index == 0 && dec->humidity.has_value()) {
          this->publish_state(*dec->humidity);
        }
      }
      break;

    case RamsesSensorType::OUTDOOR_HUMIDITY:
      if (opcode == 0x12A0) {
        auto dec = ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->sensor_index == 2 && dec->humidity.has_value()) {
          this->publish_state(*dec->humidity);
        }
      }
      break;

    case RamsesSensorType::AIR_QUALITY_TEMPERATURE:
      if (opcode == 0x12A0) {
        auto dec = ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->temperature.has_value()) {
          this->publish_state(*dec->temperature);
        }
      }
      break;

    case RamsesSensorType::BYPASS_POSITION:
      if (opcode == 0x10A0 || opcode == 0x22E5) {
        auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->bypass_position);
        }
      }
      break;

    case RamsesSensorType::FILTER_REMAINING_DAYS:
      if (opcode == 0x10D0) {
        auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->remaining_days);
        }
      }
      break;

    case RamsesSensorType::FILTER_LIFETIME_DAYS:
      if (opcode == 0x10D0) {
        auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->lifetime_days);
        }
      }
      break;

    case RamsesSensorType::FILTER_REMAINING_PERCENT:
      if (opcode == 0x10D0) {
        auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->remaining_percent);
        }
      }
      break;

    case RamsesSensorType::OPENTHERM_MODULATION:
      if (opcode == 0x3220) {
        auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->modulation_percent);
        }
      }
      break;

    case RamsesSensorType::OPENTHERM_FLOW_TEMP:
      if (opcode == 0x3220) {
        auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->flow_temp.has_value()) {
          this->publish_state(*dec->flow_temp);
        }
      }
      break;

    case RamsesSensorType::OPENTHERM_RETURN_TEMP:
      if (opcode == 0x3220) {
        auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value() && dec->return_temp.has_value()) {
          this->publish_state(*dec->return_temp);
        }
      }
      break;

    case RamsesSensorType::BATTERY_LEVEL:
      if (opcode == 0x1060) {
        auto dec = ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->battery_percent);
        }
      }
      break;
  }
}

void RamsesBinarySensor::on_message(const ramses_esp::RamsesMessage &msg) {
  if (!address_matches(this->device_address_, msg)) return;

  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  switch (this->sensor_type_) {
    case RamsesBinarySensorType::FILTER_ALARM:
      if (opcode == 0x10A0) {
        auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->filter_dirty);
        }
      } else if (opcode == 0x10D0) {
        auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->remaining_days == 0);
        }
      }
      break;

    case RamsesBinarySensorType::FLAME_ACTIVE:
      if (opcode == 0x3220) {
        auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->flame_active);
        }
      }
      break;

    case RamsesBinarySensorType::FAULT_ALARM:
      if (opcode == 0x3220) {
        auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->fault_active);
        }
      }
      break;

    case RamsesBinarySensorType::WINDOW_OPEN:
      if (opcode == 0x12B0) {
        auto dec = ramses_esp::ContactSensorPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          if (!this->zone_index_.has_value() || dec->zone_index == *this->zone_index_) {
            this->publish_state(dec->is_open);
          }
        }
      }
      break;

    case RamsesBinarySensorType::BYPASS_ACTIVE:
      if (opcode == 0x10A0 || opcode == 0x22E5) {
        auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->bypass_active);
        }
      }
      break;

    case RamsesBinarySensorType::BATTERY_LOW:
      if (opcode == 0x1060) {
        auto dec = ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
        if (dec.has_value()) {
          this->publish_state(dec->battery_low);
        }
      }
      break;
  }
}

} // namespace ramses_devices
} // namespace esphome
