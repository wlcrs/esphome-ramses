#include "ramses_climate.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_climate";

void RamsesClimate::setup() {
#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback([this](const ramses_esp::RamsesMessage &msg) {
      this->on_message(msg);
    });
  }
#endif
}

climate::ClimateTraits RamsesClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supported_modes({
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_AUTO
  });
  traits.set_supported_presets({
    climate::CLIMATE_PRESET_NONE,
    climate::CLIMATE_PRESET_HOME,
    climate::CLIMATE_PRESET_AWAY,
    climate::CLIMATE_PRESET_ECO,
    climate::CLIMATE_PRESET_COMFORT
  });
  traits.set_visual_min_temperature(5.0f);
  traits.set_visual_max_temperature(35.0f);
  traits.set_visual_temperature_step(0.5f);
  return traits;
}

static inline bool climate_address_matches(const ramses_esp::RamsesAddress &configured, const ramses_esp::RamsesMessage &msg) {
  if (!configured.is_valid) return true;
  ramses_esp::RamsesAddress src = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
  if (src == configured) return true;
  if (msg.fields & RAMSES_F_ADDR2) {
    ramses_esp::RamsesAddress targ = ramses_esp::RamsesAddress::from_bytes(msg.addr[2]);
    if (targ == configured) return true;
  }
  return false;
}

void RamsesClimate::on_message(const ramses_esp::RamsesMessage &msg) {
  if (!climate_address_matches(this->controller_address_, msg)) return;

  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  if (opcode == 0x30C9) {
    auto dec = ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      for (const auto &item : dec->zones) {
        if (item.zone_index == this->zone_index_) {
          if (item.is_valid) {
            this->current_temperature = item.temperature;
            this->publish_state();
          }
          break;
        }
      }
    }
  } else if (opcode == 0x2309) {
    auto dec = ramses_esp::SetpointPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      for (const auto &item : dec->zones) {
        if (item.zone_index == this->zone_index_) {
          if (item.is_valid) {
            this->target_temperature = item.setpoint;
            this->publish_state();
          }
          break;
        }
      }
    }
  } else if (opcode == 0x2E04) {
    auto dec = ramses_esp::SystemModePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      switch (dec->mode) {
        case ramses_esp::SystemMode::HEAT_OFF:
          this->mode = climate::CLIMATE_MODE_OFF;
          this->preset = climate::CLIMATE_PRESET_NONE;
          break;
        case ramses_esp::SystemMode::AWAY:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->preset = climate::CLIMATE_PRESET_AWAY;
          break;
        case ramses_esp::SystemMode::ECO_BOOST:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->preset = climate::CLIMATE_PRESET_ECO;
          break;
        case ramses_esp::SystemMode::DAY_OFF:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->preset = climate::CLIMATE_PRESET_HOME;
          break;
        case ramses_esp::SystemMode::DAY_OFF_ECO:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->preset = climate::CLIMATE_PRESET_HOME;
          break;
        case ramses_esp::SystemMode::AUTO_WITH_RESET:
          this->mode = climate::CLIMATE_MODE_AUTO;
          this->preset = climate::CLIMATE_PRESET_NONE;
          break;
        case ramses_esp::SystemMode::CUSTOM:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->preset = climate::CLIMATE_PRESET_COMFORT;
          break;
        case ramses_esp::SystemMode::AUTO:
        default:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->preset = climate::CLIMATE_PRESET_NONE;
          break;
      }
      this->publish_state();
    }
  } else if (opcode == 0x3150) {
    auto dec = ramses_esp::HeatDemandPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value() && dec->domain_or_zone_index == this->zone_index_) {
      if (dec->demand_percent > 0.0f) {
        this->action = climate::CLIMATE_ACTION_HEATING;
      } else {
        this->action = climate::CLIMATE_ACTION_IDLE;
      }
      this->publish_state();
    }
  }
}

void RamsesClimate::control(const climate::ClimateCall &call) {
  ramses_esp::RamsesAddress hgi_src;
  hgi_src.dev_class = 18;
  hgi_src.id = 0x005612;
  hgi_src.is_valid = true;

  if (call.get_target_temperature().has_value()) {
    float new_sp = *call.get_target_temperature();
    this->target_temperature = new_sp;
    
    ramses_esp::RamsesMessage msg = ramses_esp::SetpointPayload::encode_write(
        hgi_src, this->controller_address_, this->zone_index_, new_sp);

#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->send_message(msg);
    }
#endif
  }

  if (call.get_mode().has_value()) {
    climate::ClimateMode new_mode = *call.get_mode();
    this->mode = new_mode;

    ramses_esp::SystemMode sys_mode = ramses_esp::SystemMode::AUTO;
    if (new_mode == climate::CLIMATE_MODE_OFF) {
      sys_mode = ramses_esp::SystemMode::HEAT_OFF;
    } else if (new_mode == climate::CLIMATE_MODE_AUTO) {
      sys_mode = ramses_esp::SystemMode::AUTO_WITH_RESET;
    }

    ramses_esp::RamsesMessage msg = ramses_esp::SystemModePayload::encode_write(
        hgi_src, this->controller_address_, sys_mode);

#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->send_message(msg);
    }
#endif
  }

  if (call.get_preset().has_value()) {
    climate::ClimatePreset new_preset = *call.get_preset();
    this->preset = new_preset;

    ramses_esp::SystemMode sys_mode = ramses_esp::SystemMode::AUTO;
    switch (new_preset) {
      case climate::CLIMATE_PRESET_AWAY: sys_mode = ramses_esp::SystemMode::AWAY; break;
      case climate::CLIMATE_PRESET_ECO: sys_mode = ramses_esp::SystemMode::ECO_BOOST; break;
      case climate::CLIMATE_PRESET_HOME: sys_mode = ramses_esp::SystemMode::DAY_OFF; break;
      case climate::CLIMATE_PRESET_COMFORT: sys_mode = ramses_esp::SystemMode::CUSTOM; break;
      case climate::CLIMATE_PRESET_NONE:
      default:
        sys_mode = ramses_esp::SystemMode::AUTO;
        break;
    }

    ramses_esp::RamsesMessage msg = ramses_esp::SystemModePayload::encode_write(
        hgi_src, this->controller_address_, sys_mode);

#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->send_message(msg);
    }
#endif
  }

  this->publish_state();
}

} // namespace ramses_devices
} // namespace esphome
