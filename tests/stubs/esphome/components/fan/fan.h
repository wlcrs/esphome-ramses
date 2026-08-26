#pragma once

#include "esphome/core/component.h"
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace fan {

class Fan;

class FanTraits {
public:
  void set_speed(bool val) { speed_ = val; }
  void set_supported_preset_modes(const std::set<std::string> &modes) {
    supported_preset_modes_ = modes;
  }
  bool supports_speed() const { return speed_; }
  bool supports_preset_modes() const {
    return !supported_preset_modes_.empty();
  }

private:
  bool speed_{true};
  std::set<std::string> supported_preset_modes_;
};

class FanCall {
public:
  explicit FanCall(Fan *parent) : parent_(parent) {}

  FanCall &set_state(bool state) {
    state_ = state;
    return *this;
  }
  FanCall &set_speed(int speed) {
    speed_ = speed;
    return *this;
  }
  FanCall &set_preset_mode(const std::string &preset) {
    preset_mode_ = preset;
    return *this;
  }

  std::optional<bool> get_state() const { return state_; }
  std::optional<int> get_speed() const { return speed_; }
  const char *get_preset_mode() const { return preset_mode_.c_str(); }
  bool has_preset_mode() const { return !preset_mode_.empty(); }

  void perform();

private:
  Fan *parent_;
  std::optional<bool> state_;
  std::optional<int> speed_;
  std::string preset_mode_;
};

class Fan {
public:
  virtual ~Fan() = default;

  bool state{false};
  int speed{0};
  std::string preset_mode;

  void set_supported_preset_modes(std::initializer_list<const char *> modes) {}
  void set_preset_mode_(const char *pm) { this->preset_mode = pm ? pm : ""; }
  void set_preset_mode_(const std::string &pm) { this->preset_mode = pm; }

  virtual FanTraits get_traits() = 0;
  virtual void control(const FanCall &call) = 0;
  virtual void publish_state() { this->has_state_ = true; }

  FanCall make_call() { return FanCall(this); }
  bool has_state() const { return this->has_state_; }

protected:
  bool has_state_{false};
};

inline void FanCall::perform() { this->parent_->control(*this); }

} // namespace fan
} // namespace esphome
