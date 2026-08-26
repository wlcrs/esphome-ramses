#pragma once

#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/core/component.h"
#if __has_include("esphome/components/web_server_base/web_server_base.h")
#include "esphome/components/web_server_base/web_server_base.h"
#define RAMSES_HAS_WEB_SERVER_BASE 1
#endif
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
  int8_t last_rssi{0};
  uint32_t last_seen_ms{0};
  std::set<uint16_t> seen_opcodes;
  std::map<uint8_t, DiscoveredZone> zones;
  std::map<std::string, float> last_telemetry;
  std::string associated_remote;
  std::string associated_target;
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

#ifdef RAMSES_HAS_WEB_SERVER_BASE
  void set_web_server_base(web_server_base::WebServerBase *base) {
    this->web_server_base_ = base;
  }
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;

  void on_message(const ramses_esp::RamsesMessage &msg);
  void probe_pending();
  void trigger_probe();

  std::string generate_yaml() const;
  std::string generate_device_yaml(const DiscoveredDevice &dev) const;
  std::string generate_json(uint32_t now_ms = 0) const;
  void dump_yaml() const;

  const std::map<std::string, DiscoveredDevice> &get_devices() const {
    return this->devices_;
  }

protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
#ifdef RAMSES_HAS_WEB_SERVER_BASE
  web_server_base::WebServerBase *web_server_base_{nullptr};
#endif
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
