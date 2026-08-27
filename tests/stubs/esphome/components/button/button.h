#pragma once
#include <string>

namespace esphome {
namespace button {

class Button {
public:
  virtual ~Button() = default;
  virtual void press_action() = 0;
  void press() { press_action(); }
  void set_name(const std::string &name) { name_ = name; }

protected:
  std::string name_;
};

} // namespace button
} // namespace esphome
