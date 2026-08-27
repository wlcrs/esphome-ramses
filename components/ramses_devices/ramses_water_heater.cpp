#include "ramses_water_heater.h"
#include "esphome/core/defines.h"

#ifdef USE_WATER_HEATER

#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_water_heater";

void RamsesWaterHeater::setup() { this->setup_base(); }

water_heater::WaterHeaterTraits RamsesWaterHeater::traits() {
  water_heater::WaterHeaterTraits traits;
  traits.set_min_temperature(30.0f);
  traits.set_max_temperature(60.0f);
  traits.set_target_temperature_step(0.5f);
  traits.set_supported_modes({
      water_heater::WATER_HEATER_MODE_OFF,
      water_heater::WATER_HEATER_MODE_ECO,
      water_heater::WATER_HEATER_MODE_PERFORMANCE,
      water_heater::WATER_HEATER_MODE_ELECTRIC,
  });
  return traits;
}

void RamsesWaterHeater::handle_message(const ramses_esp::RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  if (opcode == 0x1260) {
    auto dec =
        ramses_esp::DhwStatePayload::decode_temp(msg.payload, msg.n_payload);
    if (dec.has_value() && dec->current_temp_valid) {
      this->current_temperature_ = dec->current_temp;
      this->publish_state();
    }
  } else if (opcode == 0x1F41) {
    auto dec =
        ramses_esp::DhwStatePayload::decode_state(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      if (!dec->dhw_enabled) {
        this->mode_ = water_heater::WATER_HEATER_MODE_OFF;
      } else {
        this->mode_ = dec->relay_active
                          ? water_heater::WATER_HEATER_MODE_PERFORMANCE
                          : water_heater::WATER_HEATER_MODE_ECO;
      }
      this->publish_state();
    }
  }
}

void RamsesWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  ramses_esp::RamsesAddress hgi_src{
      .dev_class = 18, .id = 0x005612, .is_valid = true};

  float target = call.get_target_temperature();
  if (!std::isnan(target)) {
    this->target_temperature_ = target;

    ramses_esp::RamsesMessage msg =
        ramses_esp::DhwStatePayload::encode_write_setpoint(
            hgi_src, this->device_address_, target);

#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->send_message(msg);
    }
#endif
  }

  if (call.get_mode().has_value()) {
    auto mode = *call.get_mode();
    auto operation_mode =
        ramses_esp::DhwStatePayload::OperationMode::FOLLOW_SCHEDULE;
    if (mode == water_heater::WATER_HEATER_MODE_OFF) {
      operation_mode =
          ramses_esp::DhwStatePayload::OperationMode::PERMANENT_OFF;
    } else if (mode == water_heater::WATER_HEATER_MODE_PERFORMANCE) {
      operation_mode = ramses_esp::DhwStatePayload::OperationMode::PERMANENT_ON;
    } else if (mode == water_heater::WATER_HEATER_MODE_ELECTRIC) {
      operation_mode = ramses_esp::DhwStatePayload::OperationMode::TEMPORARY_ON;
    }
    ramses_esp::RamsesMessage msg =
        ramses_esp::DhwStatePayload::encode_write_mode(
            hgi_src, this->device_address_, operation_mode);

#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->send_message(msg);
    }
#endif
    this->mode_ = mode;
  }

  this->publish_state();
}

} // namespace ramses_devices
} // namespace esphome

#endif // USE_WATER_HEATER
