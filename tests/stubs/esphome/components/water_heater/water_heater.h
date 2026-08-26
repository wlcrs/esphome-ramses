#pragma once

#include <string>
#include <set>
#include <optional>
#include <cmath>
#include "esphome/core/component.h"

namespace esphome {
namespace water_heater {

enum WaterHeaterMode : uint32_t {
  WATER_HEATER_MODE_OFF = 0,
  WATER_HEATER_MODE_ECO = 1,
  WATER_HEATER_MODE_ELECTRIC = 2,
  WATER_HEATER_MODE_PERFORMANCE = 3,
  WATER_HEATER_MODE_HIGH_DEMAND = 4,
  WATER_HEATER_MODE_HEAT_PUMP = 5,
  WATER_HEATER_MODE_GAS = 6,
};

class WaterHeaterTraits {
 public:
  void set_supported_modes(std::set<WaterHeaterMode> modes) { supported_modes_ = modes; }
  void set_supports_current_temperature(bool val) { supports_current_temp_ = val; }
  void set_min_temperature(float min) { min_temperature_ = min; }
  void set_max_temperature(float max) { max_temperature_ = max; }
  void set_target_temperature_step(float step) { target_temperature_step_ = step; }

 private:
  std::set<WaterHeaterMode> supported_modes_;
  bool supports_current_temp_{true};
  float min_temperature_{30.0f};
  float max_temperature_{60.0f};
  float target_temperature_step_{0.5f};
};

class WaterHeater;

class WaterHeaterCall {
 public:
  explicit WaterHeaterCall(WaterHeater *parent) : parent_(parent) {}

  WaterHeaterCall &set_target_temperature(float temp) {
    target_temperature_ = temp;
    return *this;
  }
  WaterHeaterCall &set_mode(WaterHeaterMode mode) {
    mode_ = mode;
    return *this;
  }
  WaterHeaterCall &set_away(bool away) {
    away_ = away;
    return *this;
  }

  float get_target_temperature() const { return target_temperature_; }
  const std::optional<WaterHeaterMode> &get_mode() const { return mode_; }
  const std::optional<bool> &get_away() const { return away_; }

  void perform();

 protected:
  WaterHeater *parent_;
  float target_temperature_{NAN};
  std::optional<WaterHeaterMode> mode_;
  std::optional<bool> away_;
};

struct WaterHeaterCallInternal : public WaterHeaterCall {
  explicit WaterHeaterCallInternal(WaterHeater *parent) : WaterHeaterCall(parent) {}
};

class WaterHeater {
 public:
  virtual ~WaterHeater() = default;

  float get_current_temperature() const { return current_temperature_; }
  float get_target_temperature() const { return target_temperature_; }
  WaterHeaterMode get_mode() const { return mode_; }

  virtual WaterHeaterTraits traits() {
    WaterHeaterTraits t;
    t.set_min_temperature(30.0f);
    t.set_max_temperature(60.0f);
    t.set_target_temperature_step(0.5f);
    return t;
  }

  virtual WaterHeaterCallInternal make_call() { return WaterHeaterCallInternal(this); }
  virtual void control(const WaterHeaterCall &call) = 0;
  virtual void publish_state() { this->has_state_ = true; }

  bool has_state() const { return this->has_state_; }

 protected:
  float current_temperature_{NAN};
  float target_temperature_{NAN};
  WaterHeaterMode mode_{WATER_HEATER_MODE_ECO};
  bool has_state_{false};
};

inline void WaterHeaterCall::perform() {
  this->parent_->control(*this);
}

} // namespace water_heater
} // namespace esphome
