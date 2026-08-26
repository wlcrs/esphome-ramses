#pragma once

#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/core/component.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

namespace ramses_discovery {

struct DiscoveredZone {
  uint8_t index{0};
  std::string name;
  float last_temp{0.0f};
  float last_setpoint{0.0f};
  bool has_temp{false};
  bool has_setpoint{false};
  bool name_probed{false};
};

struct DiscoveredDevice {
  ramses_esp::RamsesAddress address;
  std::string device_type; // "controller", "trv", "opentherm", "hvac",
                           // "sensor", "relay", "other"
  std::string oem_name{"generic"};
  ramses_esp::HvacScheme hvac_scheme{ramses_esp::HvacScheme::ORCON};
  bool oem_probed{false};
  bool is_hvac{false};
  bool has_dhw{false};
  bool dhw_probed{false};
  std::set<uint16_t> seen_opcodes;
  std::map<uint8_t, DiscoveredZone> zones;
  std::map<std::string, float> last_telemetry;
};

class RamsesDiscoveryComponent : public Component {
public:
  RamsesDiscoveryComponent() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) {
    this->parent_ = parent;
  }
  void set_active_probing(bool active) { this->active_probing_ = active; }
  void set_probing_interval(uint32_t interval_ms) {
    this->probing_interval_ms_ = interval_ms;
  }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void on_message(const ramses_esp::RamsesMessage &msg);
  void probe_pending();

  std::string generate_yaml() const;
  void dump_yaml() const;

  const std::map<std::string, DiscoveredDevice> &get_devices() const {
    return this->devices_;
  }

protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  bool active_probing_{true};
  uint32_t probing_interval_ms_{30000};
  uint32_t last_probe_time_{0};
  uint32_t last_dump_time_{0};

  std::map<std::string, DiscoveredDevice> devices_;

  DiscoveredDevice &get_or_create_device(const ramses_esp::RamsesAddress &addr);
  void process_controller_packet(DiscoveredDevice &dev,
                                 const ramses_esp::RamsesMessage &msg,
                                 uint16_t opcode);
  void process_hvac_packet(DiscoveredDevice &dev,
                           const ramses_esp::RamsesMessage &msg,
                           uint16_t opcode);
  void process_trv_packet(DiscoveredDevice &dev,
                          const ramses_esp::RamsesMessage &msg,
                          uint16_t opcode);
  void process_opentherm_packet(DiscoveredDevice &dev,
                                const ramses_esp::RamsesMessage &msg,
                                uint16_t opcode);
  void process_sensor_packet(DiscoveredDevice &dev,
                             const ramses_esp::RamsesMessage &msg,
                             uint16_t opcode);
};

} // namespace ramses_discovery
} // namespace esphome
