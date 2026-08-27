#pragma once

#include "esphome/core/defines.h"

#ifdef USE_FAN

#include "components/ramses_devices/ramses_entity.h"
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/components/button/button.h"
#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"
#include <set>
#include <string>

#ifdef USE_ESP_IDF
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#endif

namespace esphome {
namespace ramses_devices {

// Compact serialisation: upper 10 bits = dev_class, lower 22 bits = device id.
// Zero = "not set / invalid" sentinel.
static inline uint32_t addr_to_u32(const ramses_esp::RamsesAddress &a) {
  if (!a.is_valid)
    return 0;
  return ((uint32_t)(a.dev_class & 0x3FF) << 22) | (a.id & 0x3FFFFF);
}
static inline ramses_esp::RamsesAddress u32_to_addr(uint32_t v) {
  if (v == 0)
    return ramses_esp::RamsesAddress{};
  return ramses_esp::RamsesAddress{
      .dev_class = static_cast<uint8_t>((v >> 22) & 0x3FF),
      .id = v & 0x3FFFFF,
      .is_valid = true,
  };
}

class RamsesFan : public fan::Fan, public Component, public RamsesEntityBase {
public:
  RamsesFan() = default;

  void set_device_address(const std::string &addr) {
    this->device_address_ = ramses_esp::RamsesAddress::from_string(addr);
    this->device_address_from_yaml_ = this->device_address_;
  }
  void set_fake_remote_address(const std::string &addr) {
    this->remote_address_ = ramses_esp::RamsesAddress::from_string(addr);
    this->remote_address_from_yaml_ = this->remote_address_;
  }
  void set_scheme(ramses_esp::HvacScheme scheme) { this->scheme_ = scheme; }

  void setup() override;
  void loop() override;
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;

  void start_pairing(uint32_t timeout_ms = 30000);
  void stop_pairing();
  bool is_pairing() const { return this->pairing_active_; }

  const ramses_esp::RamsesAddress &get_remote_address() const {
    return this->remote_address_;
  }

protected:
  bool matches(const ramses_esp::RamsesMessage &msg) const override {
    return address_matches(this->device_address_, msg) ||
           address_matches(this->remote_address_, msg);
  }

  void handle_message(const ramses_esp::RamsesMessage &msg) override;

  ramses_esp::RamsesAddress remote_address_;

  // Yaml-originating addresses — kept for mismatch detection after NVS load
  ramses_esp::RamsesAddress device_address_from_yaml_;
  ramses_esp::RamsesAddress remote_address_from_yaml_;

  ramses_esp::HvacScheme scheme_{ramses_esp::HvacScheme::ORCON};

  bool pairing_active_{false};
  uint32_t pairing_start_time_{0};
  uint32_t pairing_timeout_ms_{30000};
  uint32_t last_offer_time_{0};

#ifdef USE_ESP_IDF
  ESPPreferenceObject pref_device_addr_;
  ESPPreferenceObject pref_remote_addr_;
#endif

  ramses_esp::RamsesAddress get_effective_sender_address() const;

  // Derive a stable virtual remote address from the chip MAC (survives reflash)
  ramses_esp::RamsesAddress derive_remote_from_chip_id() const;

  void load_preferences();
  void save_device_address();
  void save_remote_address();
};

class RamsesFanPairingButton : public button::Button, public Component {
public:
  void set_fan(RamsesFan *fan) { this->fan_ = fan; }
  void set_timeout_ms(uint32_t ms) { this->timeout_ms_ = ms; }
  void setup() override {}
  void press_action() override {
    if (this->fan_ != nullptr)
      this->fan_->start_pairing(this->timeout_ms_);
  }

protected:
  RamsesFan *fan_{nullptr};
  uint32_t timeout_ms_{30000};
};

} // namespace ramses_devices
} // namespace esphome

#endif // USE_FAN
