#pragma once

#if __has_include("esphome/components/ramses_devices/ramses_devices.h")
#include "esphome/components/ramses_devices/ramses_devices.h"
#else
#include "components/ramses_devices/ramses_devices.h"
#endif

#if __has_include("esphome/components/ramses_esp/ramses_decoder.h")
#include "esphome/components/ramses_esp/ramses_decoder.h"
#else
#include "components/ramses_esp/ramses_decoder.h"
#endif

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include <deque>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifdef USE_CLIMATE
#if __has_include("esphome/components/ramses_devices/ramses_climate.h")
#include "esphome/components/ramses_devices/ramses_climate.h"
#else
#include "components/ramses_devices/ramses_climate.h"
#endif
#endif

#ifdef USE_FAN
#if __has_include("esphome/components/ramses_devices/ramses_fan.h")
#include "esphome/components/ramses_devices/ramses_fan.h"
#else
#include "components/ramses_devices/ramses_fan.h"
#endif
#endif

#ifdef USE_WATER_HEATER
#if __has_include("esphome/components/ramses_devices/ramses_water_heater.h")
#include "esphome/components/ramses_devices/ramses_water_heater.h"
#else
#include "components/ramses_devices/ramses_water_heater.h"
#endif
#endif

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

namespace ramses_discovery {

class RamsesNvsStorage {
public:
  static RamsesNvsStorage &instance() {
    static RamsesNvsStorage inst;
    return inst;
  }

  // NVS Operations (uses ESP-IDF 5 Multi-Page Blobs)
  bool save_config(const std::string &json_str);
  std::string load_config();
  bool clear_config();
  bool has_config();

  // Dynamic Entity Loader
  // Parses JSON and instantiates/registers all configured entities with App
  size_t load_and_register_entities(ramses_esp::RamsesESPComponent *hub);
  size_t instantiate_from_json(const std::string &json_str,
                               ramses_esp::RamsesESPComponent *hub);

  bool is_configured() const { return this->is_configured_; }
  size_t get_entity_count() const { return this->entity_count_; }

  bool is_device_configured(const std::string &address);
  const std::set<std::string> &get_configured_addresses();

  const std::vector<Component *> &get_dynamic_components() const {
    return this->dynamic_components_;
  }
  void add_dynamic_component(Component *comp) {
    this->dynamic_components_.push_back(comp);
  }

protected:
  RamsesNvsStorage() = default;

  bool is_configured_{false};
  size_t entity_count_{0};
  std::set<std::string> configured_addresses_;
  std::vector<Component *> dynamic_components_;

  // Persistent storage for entity names to ensure StringRef pointers remain
  // valid
  std::deque<std::string> string_pool_;

  const char *intern_string(const std::string &str) {
    this->string_pool_.push_back(str);
    return this->string_pool_.back().c_str();
  }
};

} // namespace ramses_discovery
} // namespace esphome
