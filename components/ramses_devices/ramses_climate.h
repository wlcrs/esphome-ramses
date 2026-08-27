#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CLIMATE

#include "components/ramses_devices/ramses_entity.h"
#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include <optional>
#include <string>

namespace esphome {
namespace ramses_devices {

class RamsesClimate : public climate::Climate,
                      public Component,
                      public RamsesEntityBase {
public:
  RamsesClimate() = default;

  void set_controller_address(const std::string &addr) {
    this->set_device_address(addr);
  }
  void set_zone_index(uint8_t zone) { this->zone_index_ = zone; }
  void set_zone_name(const std::string &name) { this->zone_name_ = name; }

  void setup() override;
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

protected:
  void handle_message(const ramses_esp::RamsesMessage &msg) override;

  uint8_t zone_index_{0};
  std::string zone_name_;
};

} // namespace ramses_devices
} // namespace esphome

#endif // USE_CLIMATE
