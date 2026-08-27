#pragma once

#include <string>

namespace esphome {
namespace sensor {

class Sensor {
public:
  virtual ~Sensor() = default;
  virtual void publish_state(float state) {
    this->state = state;
    this->has_state_ = true;
  }
  void set_accuracy_decimals(int8_t decimals) {
    this->accuracy_decimals_ = decimals;
  }
  int8_t get_accuracy_decimals() const { return this->accuracy_decimals_; }
  float state{0.0f};
  bool has_state() const { return this->has_state_; }

protected:
  bool has_state_{false};
  int8_t accuracy_decimals_{0};
};

} // namespace sensor
} // namespace esphome
