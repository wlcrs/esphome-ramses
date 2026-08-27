#include "ramses_devices.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <cmath>

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_devices";

#ifdef USE_SENSOR
void RamsesSensor::setup() {
  this->setup_base();
  if (!this->has_accuracy_decimals()) {
    switch (this->sensor_type_) {
    case RamsesSensorType::ZONE_TEMPERATURE:
    case RamsesSensorType::ZONE_SETPOINT:
    case RamsesSensorType::OUTDOOR_TEMPERATURE:
    case RamsesSensorType::SUPPLY_TEMPERATURE:
    case RamsesSensorType::EXHAUST_TEMPERATURE:
    case RamsesSensorType::AIR_QUALITY_TEMPERATURE:
    case RamsesSensorType::OPENTHERM_FLOW_TEMP:
    case RamsesSensorType::OPENTHERM_RETURN_TEMP:
    case RamsesSensorType::UFH_MIN_TEMP:
    case RamsesSensorType::UFH_MAX_TEMP:
    case RamsesSensorType::SPIDER_TEMPERATURE:
      this->set_accuracy_decimals(1);
      break;
    default:
      this->set_accuracy_decimals(0);
      break;
    }
  }
  if (this->get_state_class() == sensor::STATE_CLASS_NONE) {
    this->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
  }
}

void RamsesSensor::publish_state(float state) {
  int8_t decimals = this->get_accuracy_decimals();
  if (decimals >= 0 && decimals <= 4) {
    float factor = 1.0f;
    for (int i = 0; i < decimals; i++)
      factor *= 10.0f;
    state = std::round(state * factor) / factor;
  }
  sensor::Sensor::publish_state(state);
}

void RamsesSensor::handle_message(const ramses_esp::RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  switch (this->sensor_type_) {
  case RamsesSensorType::ZONE_TEMPERATURE:
    if (opcode == 0x30C9) {

      auto dec =
          ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        for (const auto &item : dec->zones) {
          if (!this->zone_index_.has_value() ||
              item.zone_index == *this->zone_index_) {
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
      auto dec =
          ramses_esp::SetpointPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        for (const auto &item : dec->zones) {
          if (!this->zone_index_.has_value() ||
              item.zone_index == *this->zone_index_) {
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
      auto dec = ramses_esp::OutdoorTemperaturePayload::decode(msg.payload,
                                                               msg.n_payload);
      if (dec.has_value() && dec->is_valid) {
        this->publish_state(dec->temperature);
      }
    } else if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->outdoor_temp.has_value()) {
        this->publish_state(*dec->outdoor_temp);
      }
    }
    break;

  case RamsesSensorType::SUPPLY_TEMPERATURE:
    if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->supply_temp.has_value()) {
        this->publish_state(*dec->supply_temp);
      }
    }
    break;

  case RamsesSensorType::EXHAUST_TEMPERATURE:
    if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->exhaust_temp.has_value()) {
        this->publish_state(*dec->exhaust_temp);
      }
    }
    break;

  case RamsesSensorType::SUPPLY_FAN_SPEED:
    if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->supply_fan_speed.has_value()) {
        this->publish_state(*dec->supply_fan_speed);
      }
    }
    break;

  case RamsesSensorType::EXHAUST_FAN_SPEED:
    if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->exhaust_fan_speed.has_value()) {
        this->publish_state(*dec->exhaust_fan_speed);
      }
    }
    break;

  case RamsesSensorType::REMAINING_MINS:
    if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->remaining_mins.has_value()) {
        this->publish_state(*dec->remaining_mins);
      }
    }
    break;

  case RamsesSensorType::HEAT_DEMAND:
    if (opcode == 0x3150) {
      auto dec =
          ramses_esp::HeatDemandPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        if (!this->zone_index_.has_value() ||
            dec->domain_or_zone_index == *this->zone_index_) {
          this->publish_state(dec->demand_percent);
        }
      }
    } else if (opcode == 0x0008) {
      auto dec =
          ramses_esp::RelayDemandPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && (!this->relay_index_.has_value() ||
                              dec->relay_index == *this->relay_index_)) {
        this->publish_state(dec->demand_percent);
      }
    }
    break;

  case RamsesSensorType::RELAY_DEMAND:
    if (opcode == 0x0008) {
      auto dec =
          ramses_esp::RelayDemandPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && (!this->relay_index_.has_value() ||
                              dec->relay_index == *this->relay_index_)) {
        this->publish_state(dec->demand_percent);
      }
    }
    break;

  case RamsesSensorType::CO2:
    if (opcode == 0x1298) {
      auto dec =
          ramses_esp::Co2SensorPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->is_valid) {
        this->publish_state(dec->co2_ppm);
      }
    } else if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->co2_ppm.has_value()) {
        this->publish_state(*dec->co2_ppm);
      }
    }
    break;

  case RamsesSensorType::INDOOR_HUMIDITY:
    if (opcode == 0x12A0) {
      auto dec =
          ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->sensor_index == 0 &&
          dec->humidity.has_value()) {
        this->publish_state(*dec->humidity);
      }
    } else if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->indoor_humidity.has_value()) {
        this->publish_state(*dec->indoor_humidity);
      }
    }
    break;

  case RamsesSensorType::OUTDOOR_HUMIDITY:
    if (opcode == 0x12A0) {
      auto dec =
          ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->sensor_index == 2 &&
          dec->humidity.has_value()) {
        this->publish_state(*dec->humidity);
      }
    } else if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->outdoor_humidity.has_value()) {
        this->publish_state(*dec->outdoor_humidity);
      }
    }
    break;

  case RamsesSensorType::AIR_QUALITY_TEMPERATURE:
    if (opcode == 0x12A0) {
      auto dec =
          ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->temperature.has_value()) {
        this->publish_state(*dec->temperature);
      }
    } else if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->indoor_temp.has_value()) {
        this->publish_state(*dec->indoor_temp);
      }
    }
    break;

  case RamsesSensorType::BYPASS_POSITION:
    if (opcode == 0x10A0 || opcode == 0x22E5) {
      auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload,
                                                            msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->bypass_position);
      }
    } else if (opcode == 0x31DA) {
      auto dec =
          ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->bypass_position.has_value()) {
        this->publish_state(*dec->bypass_position);
      }
    }
    break;

  case RamsesSensorType::FILTER_REMAINING_DAYS:
    if (opcode == 0x10D0) {
      auto dec =
          ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->remaining_days);
      }
    }
    break;

  case RamsesSensorType::FILTER_LIFETIME_DAYS:
    if (opcode == 0x10D0) {
      auto dec =
          ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->lifetime_days);
      }
    }
    break;

  case RamsesSensorType::FILTER_REMAINING_PERCENT:
    if (opcode == 0x10D0) {
      auto dec =
          ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->remaining_percent);
      }
    }
    break;

  case RamsesSensorType::OPENTHERM_MODULATION:
    if (opcode == 0x3220) {
      auto dec =
          ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->modulation_percent);
      }
    }
    break;

  case RamsesSensorType::OPENTHERM_FLOW_TEMP:
    if (opcode == 0x3220) {
      auto dec =
          ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->flow_temp.has_value()) {
        this->publish_state(*dec->flow_temp);
      }
    }
    break;

  case RamsesSensorType::OPENTHERM_RETURN_TEMP:
    if (opcode == 0x3220) {
      auto dec =
          ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value() && dec->return_temp.has_value()) {
        this->publish_state(*dec->return_temp);
      }
    }
    break;

  case RamsesSensorType::BATTERY_LEVEL:
    if (opcode == 0x1060) {
      auto dec =
          ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->battery_percent);
      }
    }
    break;

  case RamsesSensorType::ACTUATOR_MODULATION:
    if (opcode == 0x3EF0 || opcode == 0x3EF1 || opcode == 0x3B00) {
      auto dec = ramses_esp::ActuatorStatePayload::decode(
          msg.payload, msg.n_payload, opcode);
      if (dec.has_value()) {
        this->publish_state(dec->modulation_percent);
      }
    }
    break;

  case RamsesSensorType::UFH_MIN_TEMP:
    if (opcode == 0x22C9 || opcode == 0x2209) {
      auto dec = ramses_esp::UfhSetpointBoundsPayload::decode(msg.payload,
                                                              msg.n_payload);
      if (dec.has_value() && dec->min_temp.has_value()) {
        if (!this->zone_index_.has_value() ||
            dec->ufh_index == *this->zone_index_) {
          this->publish_state(*dec->min_temp);
        }
      }
    }
    break;

  case RamsesSensorType::UFH_MAX_TEMP:
    if (opcode == 0x22C9 || opcode == 0x2209) {
      auto dec = ramses_esp::UfhSetpointBoundsPayload::decode(msg.payload,
                                                              msg.n_payload);
      if (dec.has_value() && dec->max_temp.has_value()) {
        if (!this->zone_index_.has_value() ||
            dec->ufh_index == *this->zone_index_) {
          this->publish_state(*dec->max_temp);
        }
      }
    }
    break;

  case RamsesSensorType::SPIDER_TEMPERATURE:
    if (opcode == 0x4E01 || opcode == 0x4E02) {
      auto dec = ramses_esp::SpiderTemperaturesPayload::decode(msg.payload,
                                                               msg.n_payload);
      if (dec.has_value() && dec->primary_temp.has_value()) {
        this->publish_state(*dec->primary_temp);
      }
    }
    break;

  case RamsesSensorType::FAULT_CODE:
    if (opcode == 0x0418 || opcode == 0x042F || opcode == 0x0009 ||
        opcode == 0x4401) {
      auto dec = ramses_esp::SystemFaultLogPayload::decode(
          msg.payload, msg.n_payload, opcode);
      if (dec.has_value()) {
        this->publish_state(dec->fault_code);
      }
    }
    break;
  }
}
#endif // USE_SENSOR

#ifdef USE_BINARY_SENSOR
void RamsesBinarySensor::setup() { this->setup_base(); }

void RamsesBinarySensor::handle_message(const ramses_esp::RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  switch (this->sensor_type_) {
  case RamsesBinarySensorType::FILTER_ALARM:
    if (opcode == 0x10A0) {
      auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload,
                                                            msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->filter_dirty);
      }
    } else if (opcode == 0x10D0) {
      auto dec =
          ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->remaining_days == 0);
      }
    } else if (opcode == 0x31D9) {
      auto dec =
          ramses_esp::HvacFanInfoPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->filter_dirty);
      }
    }
    break;

  case RamsesBinarySensorType::FLAME_ACTIVE:
    if (opcode == 0x3220) {
      auto dec =
          ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->flame_active);
      }
    }
    break;

  case RamsesBinarySensorType::FAULT_ALARM:
    if (opcode == 0x3220) {
      auto dec =
          ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->fault_active);
      }
    } else if (opcode == 0x0418 || opcode == 0x0009 || opcode == 0x4401) {
      auto dec = ramses_esp::SystemFaultLogPayload::decode(
          msg.payload, msg.n_payload, opcode);
      if (dec.has_value()) {
        this->publish_state(dec->is_fault);
      }
    }
    break;

  case RamsesBinarySensorType::WINDOW_OPEN:
    if (opcode == 0x12B0) {
      auto dec =
          ramses_esp::WindowStatePayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        if (!this->zone_index_.has_value() ||
            dec->zone_index == *this->zone_index_) {
          this->publish_state(dec->window_open);
        }
      }
    }
    break;

  case RamsesBinarySensorType::ACTUATOR_RELAY:
    if (opcode == 0x3EF0 || opcode == 0x3EF1 || opcode == 0x3B00) {
      auto dec = ramses_esp::ActuatorStatePayload::decode(
          msg.payload, msg.n_payload, opcode);
      if (dec.has_value()) {
        this->publish_state(dec->relay_active);
      }
    }
    break;

  case RamsesBinarySensorType::BYPASS_ACTIVE:
    if (opcode == 0x10A0 || opcode == 0x22E5) {
      auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload,
                                                            msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->bypass_active);
      }
    }
    break;

  case RamsesBinarySensorType::BATTERY_LOW:
    if (opcode == 0x1060) {
      auto dec =
          ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
      if (dec.has_value()) {
        this->publish_state(dec->battery_low);
      }
    }
    break;
  }
}
#endif // USE_BINARY_SENSOR

} // namespace ramses_devices
} // namespace esphome
