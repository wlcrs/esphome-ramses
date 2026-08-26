#include "ramses_fan.h"
#include "esphome/core/log.h"
#include <algorithm>

#ifdef USE_ESP_IDF
#include "esphome/core/hal.h"
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

void RamsesFan::loop() {
  if (!this->pairing_active_) return;

  uint32_t now = millis();
  if (now - this->pairing_start_time_ > this->pairing_timeout_ms_) {
    this->stop_pairing();
    ESP_LOGW(TAG, "Ventilation pairing timed out after %u ms", (unsigned int)this->pairing_timeout_ms_);
    return;
  }

  // Broadcast 1FC9 Offer packet every 1000ms during pairing mode
  if (now - this->last_offer_time_ > 1000) {
    this->last_offer_time_ = now;
#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      ramses_esp::RamsesAddress sender = this->get_effective_sender_address();
      ramses_esp::RamsesMessage offer = ramses_esp::BindingPayload::encode_offer(sender, this->scheme_);
      ESP_LOGI(TAG, "Broadcasting 1FC9 Pairing Offer from Remote %s...", sender.to_string().c_str());
      this->parent_->send_message(offer);
    }
#endif
  }
}

void RamsesFan::start_pairing(uint32_t timeout_ms) {
  this->pairing_active_ = true;
  this->pairing_start_time_ = millis();
  this->pairing_timeout_ms_ = timeout_ms;
  this->last_offer_time_ = 0;

  if (!this->remote_address_.is_valid) {
    // Generate default virtual remote address (class 29 for switch / 37 for display switch)
    this->remote_address_.dev_class = (this->scheme_ == ramses_esp::HvacScheme::VASCO) ? 29 : 37;
    this->remote_address_.id = 0x005612;
    this->remote_address_.is_valid = true;
  }

  ESP_LOGI(TAG, "Starting 1FC9 Pairing Mode for Ventilation Unit (Remote ID: %s, Timeout: %u ms)",
           this->remote_address_.to_string().c_str(), (unsigned int)timeout_ms);
}

void RamsesFan::stop_pairing() {
  this->pairing_active_ = false;
}

ramses_esp::RamsesAddress RamsesFan::get_effective_sender_address() const {
  if (this->remote_address_.is_valid) {
    return this->remote_address_;
  }
  ramses_esp::RamsesAddress hgi_src;
  hgi_src.dev_class = (this->scheme_ == ramses_esp::HvacScheme::VASCO) ? 29 : 37;
  hgi_src.id = 0x005612;
  hgi_src.is_valid = true;
  return hgi_src;
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
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  // Handle 1FC9 Binding Handshake when pairing is active
  if (this->pairing_active_ && opcode == 0x1FC9) {
    if (msg.type == ramses_esp::RAMSES_MSG_W) {
      ramses_esp::RamsesAddress target = ramses_esp::RamsesAddress::from_bytes(msg.addr[1]);
      ramses_esp::RamsesAddress sender = this->get_effective_sender_address();

      if (target == sender) {
        ramses_esp::RamsesAddress fan_addr = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
        ESP_LOGI(TAG, "Received 1FC9 Accept from Fan Unit %s! Confirming pairing...", fan_addr.to_string().c_str());

        if (!this->device_address_.is_valid) {
          this->device_address_ = fan_addr;
        }

#ifdef USE_ESP_IDF
        if (this->parent_ != nullptr) {
          ramses_esp::RamsesMessage confirm = ramses_esp::BindingPayload::encode_confirm(sender, fan_addr);
          this->parent_->send_message(confirm);
        }
#endif
        this->stop_pairing();
        ESP_LOGI(TAG, "Pairing successfully completed with Fan Unit %s!", fan_addr.to_string().c_str());
        return;
      }
    }
  }

  if (!fan_address_matches(this->device_address_, msg)) return;

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

  ramses_esp::RamsesAddress sender = this->get_effective_sender_address();

  ramses_esp::RamsesMessage msg = ramses_esp::FanStatePayload::encode_write(
      sender, this->device_address_, target_mode, this->scheme_);

#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->send_message(msg);
  }
#endif
  this->publish_state();
}

} // namespace ramses_devices
} // namespace esphome
