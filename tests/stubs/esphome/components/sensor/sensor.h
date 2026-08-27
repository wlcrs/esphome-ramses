#pragma once

#include <string>

namespace esphome {
namespace sensor {

enum StateClass : uint8_t {
  STATE_CLASS_NONE = 0,
  STATE_CLASS_MEASUREMENT = 1,
};

class Sensor {
public:
  virtual ~Sensor() = default;
  virtual void publish_state(float state) {
    this->state = state;
    this->has_state_ = true;
  }
  void set_accuracy_decimals(int8_t decimals) {
    this->accuracy_decimals_ = decimals;
    this->has_accuracy_override_ = true;
  }
  int8_t get_accuracy_decimals() const { return this->accuracy_decimals_; }
  bool has_accuracy_decimals() const { return this->has_accuracy_override_; }

  void set_state_class(StateClass state_class) {
    this->state_class_ = state_class;
  }
  StateClass get_state_class() const { return this->state_class_; }

  float state{0.0f};
  bool has_state() const { return this->has_state_; }

protected:
  bool has_state_{false};
  bool has_accuracy_override_{false};
  int8_t accuracy_decimals_{0};
  StateClass state_class_{STATE_CLASS_NONE};
};

} // namespace sensor
} // namespace esphome
