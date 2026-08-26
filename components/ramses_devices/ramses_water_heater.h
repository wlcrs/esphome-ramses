#pragma once

#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/components/water_heater/water_heater.h"
#include "esphome/core/component.h"
#include <string>

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

namespace ramses_devices {

class RamsesWaterHeater : public water_heater::WaterHeater, public Component {
public:
  RamsesWaterHeater() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) {
    this->parent_ = parent;
  }
  void set_controller_address(const std::string &addr) {
    this->controller_address_ = ramses_esp::RamsesAddress::from_string(addr);
  }

  void setup() override;
  water_heater::WaterHeaterTraits traits() override;
  water_heater::WaterHeaterCallInternal make_call() override {
    return water_heater::WaterHeaterCallInternal(this);
  }
  void control(const water_heater::WaterHeaterCall &call) override;
  void on_message(const ramses_esp::RamsesMessage &msg);

protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress controller_address_;
};

} // namespace ramses_devices
} // namespace esphome
