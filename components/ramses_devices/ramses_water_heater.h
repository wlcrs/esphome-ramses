#pragma once

#include "esphome/core/defines.h"

#ifdef USE_WATER_HEATER

#include "components/ramses_devices/ramses_entity.h"
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/components/water_heater/water_heater.h"
#include "esphome/core/component.h"
#include <string>

namespace esphome {
namespace ramses_devices {

class RamsesWaterHeater : public water_heater::WaterHeater,
                          public Component,
                          public RamsesEntityBase {
public:
  RamsesWaterHeater() = default;

  void set_controller_address(const std::string &addr) {
    this->set_device_address(addr);
  }

  void setup() override;
  water_heater::WaterHeaterTraits traits() override;
  water_heater::WaterHeaterCallInternal make_call() override {
    return water_heater::WaterHeaterCallInternal(this);
  }
  void control(const water_heater::WaterHeaterCall &call) override;

protected:
  void handle_message(const ramses_esp::RamsesMessage &msg) override;
};

} // namespace ramses_devices
} // namespace esphome

#endif // USE_WATER_HEATER
