#include "ramses_water_heater.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_water_heater";

void RamsesWaterHeater::setup() {
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback([this](const ramses_esp::RamsesMessage &msg) {
      this->on_message(msg);
    });
  }
#endif
}

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

static inline bool dhw_address_matches(const ramses_esp::RamsesAddress &configured, const ramses_esp::RamsesMessage &msg) {
  if (!configured.is_valid) return true;
  ramses_esp::RamsesAddress src = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
  if (src == configured) return true;
  if (msg.fields & RAMSES_F_ADDR2) {
    ramses_esp::RamsesAddress targ = ramses_esp::RamsesAddress::from_bytes(msg.addr[2]);
    if (targ == configured) return true;
  }
  return false;
}

void RamsesWaterHeater::on_message(const ramses_esp::RamsesMessage &msg) {
  if (!dhw_address_matches(this->controller_address_, msg)) return;

  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  if (opcode == 0x1260) {
    auto dec = ramses_esp::DhwStatePayload::decode_temp(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      this->current_temperature_ = dec->current_temp;
      this->publish_state();
    }
  } else if (opcode == 0x1F41) {
    auto dec = ramses_esp::DhwStatePayload::decode_state(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      if (!dec->dhw_enabled) {
        this->mode_ = water_heater::WATER_HEATER_MODE_OFF;
      } else {
        this->mode_ = dec->relay_active ? water_heater::WATER_HEATER_MODE_PERFORMANCE : water_heater::WATER_HEATER_MODE_ECO;
      }
      this->publish_state();
    }
  }
}

void RamsesWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  ramses_esp::RamsesAddress hgi_src;
  hgi_src.dev_class = 18;
  hgi_src.id = 0x005612;
  hgi_src.is_valid = true;

  float target = call.get_target_temperature();
  if (!std::isnan(target)) {
    this->target_temperature_ = target;

    ramses_esp::RamsesMessage msg = ramses_esp::DhwStatePayload::encode_write_setpoint(
        hgi_src, this->controller_address_, target);

#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->send_message(msg);
    }
#endif
  }

  if (call.get_mode().has_value()) {
    this->mode_ = *call.get_mode();
  }

  this->publish_state();
}

} // namespace ramses_devices
} // namespace esphome
