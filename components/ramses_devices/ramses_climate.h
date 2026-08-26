#pragma once

#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include <optional>
#include <string>

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

namespace ramses_devices {

class RamsesClimate : public climate::Climate, public Component {
public:
  RamsesClimate() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) {
    this->parent_ = parent;
  }
  void set_controller_address(const std::string &addr) {
    this->controller_address_ = ramses_esp::RamsesAddress::from_string(addr);
  }
  void set_zone_index(uint8_t zone) { this->zone_index_ = zone; }
  void set_zone_name(const std::string &name) { this->zone_name_ = name; }

  void setup() override;
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void on_message(const ramses_esp::RamsesMessage &msg);

protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress controller_address_;
  uint8_t zone_index_{0};
  std::string zone_name_;
};

} // namespace ramses_devices
} // namespace esphome
