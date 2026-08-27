#pragma once

#include "esphome/core/component.h"
#include <string>
#include <vector>

namespace esphome {

class Application {
public:
  void register_component_(Component *c) { components_.push_back(c); }

  template <typename T>
  void register_climate(T *climate, const char *name, uint32_t = 0,
                        uint32_t = 0) {
    climates_.push_back(climate);
  }

  template <typename T>
  void register_sensor(T *sensor, const char *name, uint32_t = 0,
                       uint32_t = 0) {
    sensors_.push_back(sensor);
  }

  template <typename T>
  void register_binary_sensor(T *sensor, const char *name, uint32_t = 0,
                              uint32_t = 0) {
    binary_sensors_.push_back(sensor);
  }

  template <typename T>
  void register_fan(T *fan, const char *name, uint32_t = 0, uint32_t = 0) {
    fans_.push_back(fan);
  }

  template <typename T>
  void register_water_heater(T *wh, const char *name, uint32_t = 0,
                             uint32_t = 0) {
    water_heaters_.push_back(wh);
  }

  void safe_reboot() {}
  void reboot() {}

  std::vector<Component *> components_;
  std::vector<void *> climates_;
  std::vector<void *> sensors_;
  std::vector<void *> binary_sensors_;
  std::vector<void *> fans_;
  std::vector<void *> water_heaters_;
};

extern Application App;

} // namespace esphome
