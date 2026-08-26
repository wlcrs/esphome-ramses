#pragma once

#include <string>

namespace esphome {
namespace binary_sensor {

class BinarySensor {
public:
  virtual ~BinarySensor() = default;
  virtual void publish_state(bool state) {
    this->state = state;
    this->has_state_ = true;
  }
  bool state{false};
  bool has_state() const { return this->has_state_; }

protected:
  bool has_state_{false};
};

} // namespace binary_sensor
} // namespace esphome
