#pragma once

#include <cstdint>
#include <string>

namespace esphome {

class Component {
public:
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
};

inline uint32_t millis() { return 0; }

} // namespace esphome
