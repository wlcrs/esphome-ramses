#include "ramses_discovery.h"
#include "esphome/core/log.h"
#include <sstream>
#include <iomanip>

#ifdef USE_ESP_IDF
#include "esphome/core/hal.h"
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_discovery {

static const char *const TAG = "ramses_discovery";

void RamsesDiscoveryComponent::setup() {
  ESP_LOGI(TAG, "Initializing RAMSES Auto-Discovery engine...");
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback([this](const ramses_esp::RamsesMessage &msg) {
      this->on_message(msg);
    });
  }
#endif
}

void RamsesDiscoveryComponent::loop() {
  uint32_t now = millis();
  if (this->active_probing_ && (now - this->last_probe_time_ > this->probing_interval_ms_)) {
    this->last_probe_time_ = now;
    this->probe_pending();
  }

  // Periodic discovery summary dump every 60s
  if (now - this->last_dump_time_ > 60000) {
    this->last_dump_time_ = now;
    if (!this->devices_.empty()) {
      this->dump_yaml();
    }
  }
}

void RamsesDiscoveryComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RAMSES Discovery Component:");
  ESP_LOGCONFIG(TAG, "  Active Probing: %s", this->active_probing_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Probing Interval: %u ms", (unsigned int)this->probing_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Discovered Devices: %d", (int)this->devices_.size());
}

DiscoveredDevice &RamsesDiscoveryComponent::get_or_create_device(const ramses_esp::RamsesAddress &addr) {
  std::string key = addr.to_string();
  auto it = this->devices_.find(key);
  if (it != this->devices_.end()) {
    return it->second;
  }

  DiscoveredDevice dev;
  dev.address = addr;

  // Infer device type from prefix
  switch (addr.dev_class) {
    case 1:
      dev.device_type = "controller";
      break;
    case 4:
      dev.device_type = "trv";
      break;
    case 10:
      dev.device_type = "opentherm";
      break;
    case 13:
      dev.device_type = "relay";
      break;
    case 18:
      dev.device_type = "gateway";
      break;
    case 22:
    case 34:
      dev.device_type = "sensor";
      break;
    case 32:
    case 37:
    case 29:
      dev.device_type = "hvac";
      dev.is_hvac = true;
      break;
    default:
      dev.device_type = "other";
      break;
  }

  this->devices_[key] = dev;
  return this->devices_[key];
}

void RamsesDiscoveryComponent::on_message(const ramses_esp::RamsesMessage &msg) {
  ramses_esp::RamsesAddress src = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
  if (!src.is_valid) return;

  DiscoveredDevice &dev = this->get_or_create_device(src);
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  dev.seen_opcodes.insert(opcode);

  if (dev.device_type == "controller") {
    this->process_controller_packet(dev, msg, opcode);
  } else if (dev.device_type == "hvac" || dev.is_hvac) {
    this->process_hvac_packet(dev, msg, opcode);
  } else if (dev.device_type == "trv") {
    this->process_trv_packet(dev, msg, opcode);
  } else if (dev.device_type == "opentherm") {
    this->process_opentherm_packet(dev, msg, opcode);
  } else {
    this->process_sensor_packet(dev, msg, opcode);
  }
}

void RamsesDiscoveryComponent::process_controller_packet(DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg, uint16_t opcode) {
  if (opcode == 0x30C9) {
    auto dec = ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      for (const auto &item : dec->zones) {
        DiscoveredZone &zone = dev.zones[item.zone_index];
        zone.index = item.zone_index;
        zone.last_temp = item.temperature;
        zone.has_temp = true;
      }
    }
  } else if (opcode == 0x2309) {
    auto dec = ramses_esp::SetpointPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      for (const auto &item : dec->zones) {
        DiscoveredZone &zone = dev.zones[item.zone_index];
        zone.index = item.zone_index;
        zone.last_setpoint = item.setpoint;
        zone.has_setpoint = true;
      }
    }
  } else if (opcode == 0x0004) {
    auto dec = ramses_esp::ZoneNamePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      DiscoveredZone &zone = dev.zones[dec->zone_index];
      zone.index = dec->zone_index;
      zone.name = dec->name;
      zone.name_probed = true;
      ESP_LOGI(TAG, "Discovered Zone %u Name: '%s' for Controller %s",
               dec->zone_index, dec->name.c_str(), dev.address.to_string().c_str());
    }
  } else if (opcode == 0x1260 || opcode == 0x12F0 || opcode == 0x1F41) {
    dev.has_dhw = true;
  }
}

void RamsesDiscoveryComponent::process_hvac_packet(DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg, uint16_t opcode) {
  if (opcode == 0x10E0) {
    auto dec = ramses_esp::DeviceInfoPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.oem_probed = true;
      if (dec->oem_code == 0x67) {
        dev.oem_name = "orcon";
        dev.hvac_scheme = ramses_esp::HvacScheme::ORCON;
      } else if (dec->oem_code == 0x08) {
        dev.oem_name = "itho";
        dev.hvac_scheme = ramses_esp::HvacScheme::ITHO;
      } else if (dec->oem_code == 0x13) {
        dev.oem_name = "vasco";
        dev.hvac_scheme = ramses_esp::HvacScheme::VASCO;
      } else if (dec->oem_code == 0x02) {
        dev.oem_name = "zehnder";
        dev.hvac_scheme = ramses_esp::HvacScheme::ZEHNDER;
      }
      ESP_LOGI(TAG, "Identified HVAC Unit %s OEM Scheme: %s",
               dev.address.to_string().c_str(), dev.oem_name.c_str());
    }
  } else if (opcode == 0x10D0) {
    auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["filter_remaining_days"] = static_cast<float>(dec->remaining_days);
    }
  } else if (opcode == 0x1298) {
    auto dec = ramses_esp::Co2SensorPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["co2"] = static_cast<float>(dec->co2_ppm);
    }
  } else if (opcode == 0x12A0) {
    auto dec = ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      if (dec->temperature.has_value()) dev.last_telemetry["indoor_temperature"] = *dec->temperature;
      if (dec->humidity.has_value()) dev.last_telemetry["indoor_humidity"] = *dec->humidity;
    }
  }
}

void RamsesDiscoveryComponent::process_trv_packet(DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg, uint16_t opcode) {
  if (opcode == 0x3150) {
    auto dec = ramses_esp::HeatDemandPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["heat_demand"] = dec->demand_percent;
    }
  } else if (opcode == 0x1060) {
    auto dec = ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["battery_level"] = dec->battery_percent;
    }
  }
}

void RamsesDiscoveryComponent::process_opentherm_packet(DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg, uint16_t opcode) {
  if (opcode == 0x3220) {
    auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["modulation"] = dec->modulation_percent;
      if (dec->flow_temp.has_value()) dev.last_telemetry["flow_temperature"] = *dec->flow_temp;
      if (dec->return_temp.has_value()) dev.last_telemetry["return_temperature"] = *dec->return_temp;
    }
  }
}

void RamsesDiscoveryComponent::process_sensor_packet(DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg, uint16_t opcode) {
  if (opcode == 0x12C0) {
    auto dec = ramses_esp::OutdoorTemperaturePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value() && dec->is_valid) {
      dev.last_telemetry["outdoor_temperature"] = dec->temperature;
    }
  }
}

void RamsesDiscoveryComponent::probe_pending() {
#ifdef USE_ESP_IDF
  if (this->parent_ == nullptr) return;

  ramses_esp::RamsesAddress hgi_src;
  hgi_src.dev_class = 18;
  hgi_src.id = 0x005612;
  hgi_src.is_valid = true;

  for (auto &kv : this->devices_) {
    DiscoveredDevice &dev = kv.second;

    // Probe un-named zones on controllers
    if (dev.device_type == "controller") {
      for (auto &zkv : dev.zones) {
        DiscoveredZone &zone = zkv.second;
        if (zone.name.empty() && !zone.name_probed) {
          zone.name_probed = true;
          ramses_esp::RamsesMessage query = ramses_esp::ZoneNamePayload::encode_query(hgi_src, dev.address, zone.index);
          this->parent_->send_message(query);
          return; // Send one probe per loop to avoid RF collisions
        }
      }
    }

    // Probe OEM signature on HVAC units
    if (dev.is_hvac && !dev.oem_probed) {
      dev.oem_probed = true;
      ramses_esp::RamsesMessage query = ramses_esp::DeviceInfoPayload::encode_query(hgi_src, dev.address);
      this->parent_->send_message(query);
      return;
    }
  }
#endif
}

std::string RamsesDiscoveryComponent::generate_yaml() const {
  std::stringstream ss;

  ss << "# ==========================================================\n";
  ss << "# Automatically Generated RAMSES Configuration\n";
  ss << "# Generated by ramses_discovery\n";
  ss << "# ==========================================================\n\n";

  // 1. Climate Platform
  bool has_climate = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.device_type == "controller" && !dev.zones.empty()) {
      if (!has_climate) {
        ss << "climate:\n";
        has_climate = true;
      }
      for (const auto &zkv : dev.zones) {
        const DiscoveredZone &z = zkv.second;
        std::string zone_name = z.name.empty() ? ("Zone " + std::to_string(z.index)) : z.name;
        ss << "  - platform: ramses_devices\n";
        ss << "    ramses_esp_id: ramses_hub\n";
        ss << "    name: \"" << zone_name << " Heating\"\n";
        ss << "    controller_address: \"" << dev.address.to_string() << "\"\n";
        ss << "    zone_index: " << (int)z.index << "\n";
        ss << "    zone_name: \"" << zone_name << "\"\n\n";
      }
    }
  }

  // 2. Water Heater Platform
  bool has_dhw = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.has_dhw) {
      if (!has_dhw) {
        ss << "water_heater:\n";
        has_dhw = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Domestic Hot Water\"\n";
      ss << "    controller_address: \"" << dev.address.to_string() << "\"\n\n";
    }
  }

  // 3. Fan Platform
  bool has_fan = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.is_hvac) {
      if (!has_fan) {
        ss << "fan:\n";
        has_fan = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Ventilation Unit\"\n";
      ss << "    device_address: \"" << dev.address.to_string() << "\"\n";
      ss << "    scheme: " << dev.oem_name << "\n\n";
    }
  }

  // 4. Sensor Platform
  bool has_sensor = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();

    if (dev.device_type == "trv") {
      if (!has_sensor) { ss << "sensor:\n"; has_sensor = true; }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: heat_demand\n";
      ss << "    name: \"TRV " << addr_str << " Heat Demand\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: battery_level\n";
      ss << "    name: \"TRV " << addr_str << " Battery Level\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.device_type == "opentherm") {
      if (!has_sensor) { ss << "sensor:\n"; has_sensor = true; }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: opentherm_modulation\n";
      ss << "    name: \"Boiler Modulation\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: flow_temperature\n";
      ss << "    name: \"Boiler Flow Temperature\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: return_temperature\n";
      ss << "    name: \"Boiler Return Temperature\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.is_hvac) {
      if (!has_sensor) { ss << "sensor:\n"; has_sensor = true; }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: filter_remaining_days\n";
      ss << "    name: \"Ventilation Filter Remaining Days\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    }
  }

  // 5. Binary Sensor Platform
  bool has_bin = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();

    if (dev.device_type == "trv") {
      if (!has_bin) { ss << "binary_sensor:\n"; has_bin = true; }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: battery_low\n";
      ss << "    name: \"TRV " << addr_str << " Battery Low Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.device_type == "opentherm") {
      if (!has_bin) { ss << "binary_sensor:\n"; has_bin = true; }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: flame_active\n";
      ss << "    name: \"Boiler Flame Active\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";

      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: fault_alarm\n";
      ss << "    name: \"Boiler Fault Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    } else if (dev.is_hvac) {
      if (!has_bin) { ss << "binary_sensor:\n"; has_bin = true; }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: filter_alarm\n";
      ss << "    name: \"Ventilation Filter Dirty Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n\n";
    }
  }

  return ss.str();
}

void RamsesDiscoveryComponent::dump_yaml() const {
  std::string yaml = this->generate_yaml();
  ESP_LOGI(TAG, "\n%s", yaml.c_str());
}

} // namespace ramses_discovery
} // namespace esphome
