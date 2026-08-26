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
  float state{0.0f};
  bool has_state() const { return this->has_state_; }

protected:
  bool has_state_{false};
};

} // namespace sensor
} // namespace esphome
