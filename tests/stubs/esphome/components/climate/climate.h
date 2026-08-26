#pragma once

#include <cmath>
#include <set>
#include <optional>
#include "esphome/core/component.h"

namespace esphome {
namespace climate {

enum ClimateMode : uint8_t {
  CLIMATE_MODE_OFF = 0,
  CLIMATE_MODE_HEAT = 1,
  CLIMATE_MODE_AUTO = 2,
};

enum ClimateAction : uint8_t {
  CLIMATE_ACTION_OFF = 0,
  CLIMATE_ACTION_IDLE = 1,
  CLIMATE_ACTION_HEATING = 2,
};

enum ClimatePreset : uint8_t {
  CLIMATE_PRESET_NONE = 0,
  CLIMATE_PRESET_HOME = 1,
  CLIMATE_PRESET_AWAY = 2,
  CLIMATE_PRESET_ECO = 3,
  CLIMATE_PRESET_COMFORT = 4,
};

class ClimateTraits {
 public:
  void set_supported_modes(std::set<ClimateMode> modes) { supported_modes_ = modes; }
  void set_supported_presets(std::set<ClimatePreset> presets) { supported_presets_ = presets; }
  void set_visual_min_temperature(float min) { visual_min_temperature_ = min; }
  void set_visual_max_temperature(float max) { visual_max_temperature_ = max; }
  void set_visual_temperature_step(float step) { visual_temperature_step_ = step; }

 private:
  std::set<ClimateMode> supported_modes_;
  std::set<ClimatePreset> supported_presets_;
  float visual_min_temperature_{5.0f};
  float visual_max_temperature_{35.0f};
  float visual_temperature_step_{0.5f};
};

class Climate;

class ClimateCall {
 public:
  explicit ClimateCall(Climate *parent) : parent_(parent) {}

  ClimateCall &set_target_temperature(float temp) {
    target_temperature_ = temp;
    return *this;
  }
  ClimateCall &set_mode(ClimateMode mode) {
    mode_ = mode;
    return *this;
  }
  ClimateCall &set_preset(ClimatePreset preset) {
    preset_ = preset;
    return *this;
  }

  std::optional<float> get_target_temperature() const { return target_temperature_; }
  std::optional<ClimateMode> get_mode() const { return mode_; }
  std::optional<ClimatePreset> get_preset() const { return preset_; }

  void perform();

 private:
  Climate *parent_;
  std::optional<float> target_temperature_;
  std::optional<ClimateMode> mode_;
  std::optional<ClimatePreset> preset_;
};

class Climate {
 public:
  virtual ~Climate() = default;

  float current_temperature{NAN};
  float target_temperature{NAN};
  ClimateMode mode{CLIMATE_MODE_HEAT};
  ClimateAction action{CLIMATE_ACTION_IDLE};
  std::optional<ClimatePreset> preset{CLIMATE_PRESET_NONE};

  virtual ClimateTraits traits() {
    ClimateTraits t;
    t.set_visual_min_temperature(5.0f);
    t.set_visual_max_temperature(35.0f);
    t.set_visual_temperature_step(0.5f);
    return t;
  }

  virtual void control(const ClimateCall &call) = 0;
  virtual void publish_state() { this->has_state_ = true; }

  ClimateCall make_call() { return ClimateCall(this); }
  bool has_state() const { return this->has_state_; }

 protected:
  bool has_state_{false};
};

inline void ClimateCall::perform() {
  this->parent_->control(*this);
}

} // namespace climate
} // namespace esphome
