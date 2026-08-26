#pragma once

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "components/ramses_esp/ramses_message.h"
#include "components/ramses_esp/ramses_decoder.h"
#include <string>
#include <set>

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

namespace ramses_devices {

class RamsesFan : public fan::Fan, public Component {
 public:
  RamsesFan() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) { this->parent_ = parent; }
  void set_device_address(const std::string &addr) { this->device_address_ = ramses_esp::RamsesAddress::from_string(addr); }
  void set_fake_remote_address(const std::string &addr) { this->remote_address_ = ramses_esp::RamsesAddress::from_string(addr); }
  void set_scheme(ramses_esp::HvacScheme scheme) { this->scheme_ = scheme; }

  void setup() override;
  void loop() override;
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
  void on_message(const ramses_esp::RamsesMessage &msg);

  void start_pairing(uint32_t timeout_ms = 30000);
  void stop_pairing();
  bool is_pairing() const { return this->pairing_active_; }

  const ramses_esp::RamsesAddress &get_device_address() const { return this->device_address_; }
  const ramses_esp::RamsesAddress &get_remote_address() const { return this->remote_address_; }

 protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress device_address_;
  ramses_esp::RamsesAddress remote_address_;
  ramses_esp::HvacScheme scheme_{ramses_esp::HvacScheme::ORCON};

  bool pairing_active_{false};
  uint32_t pairing_start_time_{0};
  uint32_t pairing_timeout_ms_{30000};
  uint32_t last_offer_time_{0};

  ramses_esp::RamsesAddress get_effective_sender_address() const;
};

} // namespace ramses_devices
} // namespace esphome
