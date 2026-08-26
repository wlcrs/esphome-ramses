#include "ramses_fan.h"
#include "esphome/core/log.h"
#include <algorithm>

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_fan";

void RamsesFan::setup() {
  this->set_supported_preset_modes({"auto", "low", "medium", "high", "boost", "away"});
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback([this](const ramses_esp::RamsesMessage &msg) {
      this->on_message(msg);
    });
  }
#endif
}

fan::FanTraits RamsesFan::get_traits() {
  fan::FanTraits traits;
  traits.set_speed(true);
  return traits;
}

static inline bool fan_address_matches(const ramses_esp::RamsesAddress &configured, const ramses_esp::RamsesMessage &msg) {
  if (!configured.is_valid) return true;
  ramses_esp::RamsesAddress src = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
  if (src == configured) return true;
  if (msg.fields & RAMSES_F_ADDR2) {
    ramses_esp::RamsesAddress targ = ramses_esp::RamsesAddress::from_bytes(msg.addr[2]);
    if (targ == configured) return true;
  }
  return false;
}

void RamsesFan::on_message(const ramses_esp::RamsesMessage &msg) {
  if (!fan_address_matches(this->device_address_, msg)) return;

  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  if (opcode == 0x22F1 || opcode == 0x22F3) {
    auto dec = ramses_esp::FanStatePayload::decode(msg.payload, msg.n_payload, this->scheme_);
    if (dec.has_value()) {
      this->set_preset_mode_(ramses_esp::fan_preset_to_string(dec->preset_mode));
      this->state = (dec->preset_mode != ramses_esp::FanPresetMode::OFF);
      this->speed = dec->speed_percent;
      this->publish_state();
    }
  }
}

void RamsesFan::control(const fan::FanCall &call) {
  ramses_esp::FanPresetMode target_mode = ramses_esp::FanPresetMode::AUTO;

  if (call.has_preset_mode()) {
    std::string pm = call.get_preset_mode();
    std::transform(pm.begin(), pm.end(), pm.begin(), ::tolower);
    if (pm == "auto") target_mode = ramses_esp::FanPresetMode::AUTO;
    else if (pm == "low") target_mode = ramses_esp::FanPresetMode::LOW;
    else if (pm == "medium" || pm == "med") target_mode = ramses_esp::FanPresetMode::MEDIUM;
    else if (pm == "high") target_mode = ramses_esp::FanPresetMode::HIGH;
    else if (pm == "boost") target_mode = ramses_esp::FanPresetMode::BOOST;
    else if (pm == "away") target_mode = ramses_esp::FanPresetMode::AWAY;
    else if (pm == "off") target_mode = ramses_esp::FanPresetMode::OFF;
  } else if (call.get_state().has_value()) {
    if (!*call.get_state()) {
      target_mode = (this->scheme_ == ramses_esp::HvacScheme::ORCON) ? ramses_esp::FanPresetMode::LOW : ramses_esp::FanPresetMode::OFF;
    } else {
      target_mode = ramses_esp::FanPresetMode::AUTO;
    }
  } else if (call.get_speed().has_value()) {
    int sp = *call.get_speed();
    if (sp == 0) target_mode = ramses_esp::FanPresetMode::OFF;
    else if (sp <= 33) target_mode = ramses_esp::FanPresetMode::LOW;
    else if (sp <= 66) target_mode = ramses_esp::FanPresetMode::MEDIUM;
    else if (sp <= 90) target_mode = ramses_esp::FanPresetMode::HIGH;
    else target_mode = ramses_esp::FanPresetMode::BOOST;
  }

  this->set_preset_mode_(ramses_esp::fan_preset_to_string(target_mode));
  this->state = (target_mode != ramses_esp::FanPresetMode::OFF);
  if (target_mode == ramses_esp::FanPresetMode::LOW) this->speed = 33;
  else if (target_mode == ramses_esp::FanPresetMode::MEDIUM) this->speed = 66;
  else if (target_mode == ramses_esp::FanPresetMode::HIGH || target_mode == ramses_esp::FanPresetMode::BOOST) this->speed = 100;
  else if (target_mode == ramses_esp::FanPresetMode::OFF) this->speed = 0;

  ramses_esp::RamsesAddress hgi_src;
  hgi_src.dev_class = 18;
  hgi_src.id = 0x005612;
  hgi_src.is_valid = true;

  ramses_esp::RamsesMessage msg = ramses_esp::FanStatePayload::encode_write(
      hgi_src, this->device_address_, target_mode, this->scheme_);

#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->send_message(msg);
  }
#endif

  this->publish_state();
}

} // namespace ramses_devices
} // namespace esphome
