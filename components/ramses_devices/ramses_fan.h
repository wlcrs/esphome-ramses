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
  void set_scheme(ramses_esp::HvacScheme scheme) { this->scheme_ = scheme; }

  void setup() override;
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
  void on_message(const ramses_esp::RamsesMessage &msg);

 protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress device_address_;
  ramses_esp::HvacScheme scheme_{ramses_esp::HvacScheme::ORCON};
};

} // namespace ramses_devices
} // namespace esphome
