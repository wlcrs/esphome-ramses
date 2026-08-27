#include "ramses_discovery.h"
#if __has_include("esphome/components/ramses_discovery/ramses_nvs_storage.h")
#include "esphome/components/ramses_discovery/ramses_nvs_storage.h"
#else
#include "components/ramses_discovery/ramses_nvs_storage.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <iomanip>
#include <sstream>

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#include "esphome/core/hal.h"
#endif

namespace esphome {
namespace ramses_discovery {

static const char *const TAG = "ramses_discovery";

static const char RAMSES_DISCOVERY_FALLBACK_HTML[] =
    "<!DOCTYPE html><html><body><h1>RAMSES Discovery</h1><p>Dashboard not "
    "loaded.</p></body></html>";

static inline std::string format_float_val(float val, int decimals) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(decimals) << val;
  return ss.str();
}

static inline std::string to_hex_byte(uint8_t b) {
  char buf[4];
  snprintf(buf, sizeof(buf), "%02X", b);
  return buf;
}

void RamsesDiscoveryComponent::decode_packet_details(
    const ramses_esp::RamsesMessage &msg, DiscoveredPacket &pkt) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  char hex_buf[8];
  snprintf(hex_buf, sizeof(hex_buf), "%04X", opcode);
  pkt.opcode_hex = hex_buf;

  switch (msg.type) {
  case ramses_esp::RAMSES_MSG_I:
    pkt.verb = "I";
    break;
  case ramses_esp::RAMSES_MSG_RP:
    pkt.verb = "RP";
    break;
  case ramses_esp::RAMSES_MSG_RQ:
    pkt.verb = "RQ";
    break;
  case ramses_esp::RAMSES_MSG_W:
    pkt.verb = "W";
    break;
  default:
    pkt.verb = "?";
    break;
  }

  if (msg.fields & RAMSES_F_ADDR0) {
    pkt.src = ramses_esp::RamsesAddress::from_bytes(msg.addr[0]).to_string();
  } else {
    pkt.src = "--:------";
  }

  if (msg.fields & RAMSES_F_ADDR1) {
    pkt.dst = ramses_esp::RamsesAddress::from_bytes(msg.addr[1]).to_string();
  } else {
    pkt.dst = "--:------";
  }

  // Resolve HVAC scheme from sender, receiver, or discovered devices
  ramses_esp::HvacScheme scheme = ramses_esp::HvacScheme::AUTO;
  if (msg.fields & RAMSES_F_ADDR0) {
    auto it = this->devices_.find(pkt.src);
    if (it != this->devices_.end() &&
        it->second.hvac_scheme != ramses_esp::HvacScheme::AUTO) {
      scheme = it->second.hvac_scheme;
    }
  }
  if (scheme == ramses_esp::HvacScheme::AUTO && (msg.fields & RAMSES_F_ADDR1)) {
    auto it = this->devices_.find(pkt.dst);
    if (it != this->devices_.end() &&
        it->second.hvac_scheme != ramses_esp::HvacScheme::AUTO) {
      scheme = it->second.hvac_scheme;
    }
  }
  if (scheme == ramses_esp::HvacScheme::AUTO) {
    for (const auto &pair : this->devices_) {
      if (pair.second.hvac_scheme != ramses_esp::HvacScheme::AUTO) {
        scheme = pair.second.hvac_scheme;
        break;
      }
    }
  }

  switch (opcode) {
  case 0x31DA: {
    pkt.opcode_name = "Ventilation Status & Telemetry";
    auto payload =
        ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.len);
    if (payload) {
      if (payload->supply_temp)
        pkt.fields.push_back(
            {"Supply Temperature",
             format_float_val(*payload->supply_temp, 1) + " °C"});
      if (payload->exhaust_temp)
        pkt.fields.push_back(
            {"Exhaust Temperature",
             format_float_val(*payload->exhaust_temp, 1) + " °C"});
      if (payload->indoor_temp)
        pkt.fields.push_back(
            {"Indoor Temperature",
             format_float_val(*payload->indoor_temp, 1) + " °C"});
      if (payload->outdoor_temp)
        pkt.fields.push_back(
            {"Outdoor Temperature",
             format_float_val(*payload->outdoor_temp, 1) + " °C"});
      if (payload->supply_fan_speed)
        pkt.fields.push_back(
            {"Supply Fan Speed",
             format_float_val(*payload->supply_fan_speed, 0) + " %"});
      if (payload->exhaust_fan_speed)
        pkt.fields.push_back(
            {"Exhaust Fan Speed",
             format_float_val(*payload->exhaust_fan_speed, 0) + " %"});
      if (payload->bypass_position)
        pkt.fields.push_back(
            {"Bypass Position",
             format_float_val(*payload->bypass_position, 0) + " %"});
      if (payload->remaining_mins)
        pkt.fields.push_back(
            {"Remaining Timer",
             std::to_string(*payload->remaining_mins) + " min"});
      if (payload->co2_ppm)
        pkt.fields.push_back(
            {"CO2 Level", std::to_string(*payload->co2_ppm) + " ppm"});
      if (payload->indoor_humidity)
        pkt.fields.push_back(
            {"Indoor Humidity",
             format_float_val(*payload->indoor_humidity, 0) + " %"});
      if (payload->outdoor_humidity)
        pkt.fields.push_back(
            {"Outdoor Humidity",
             format_float_val(*payload->outdoor_humidity, 0) + " %"});
    }
    break;
  }
  case 0x10E0: {
    pkt.opcode_name = "Device Info / Signature";
    auto payload = ramses_esp::DeviceInfoPayload::decode(msg.payload, msg.len);
    if (payload) {
      std::string oem_str =
          "Generic (0x" + to_hex_byte(payload->oem_code) + ")";
      if (payload->oem_code == 0x67)
        oem_str = "Orcon (0x67)";
      else if (payload->oem_code == 0x6A)
        oem_str = "Hopper / Brofer (0x6A)";
      else if (payload->oem_code == 0x13 || payload->oem_code == 0x66)
        oem_str = "Vasco (0x" + to_hex_byte(payload->oem_code) + ")";
      else if (payload->oem_code == 0x08 || payload->oem_code == 0x01)
        oem_str = "Itho Daalderop (0x" + to_hex_byte(payload->oem_code) + ")";
      else if (payload->oem_code == 0x02)
        oem_str = "Zehnder (0x02)";
      pkt.fields.push_back({"OEM Scheme", oem_str});
      std::string ascii;
      for (size_t i = 0; i < msg.len; i++) {
        char c = static_cast<char>(msg.payload[i]);
        if (c >= 32 && c <= 126)
          ascii += c;
      }
      if (!ascii.empty()) {
        pkt.fields.push_back({"Model Signature", ascii});
      }
    }
    break;
  }
  case 0x30C9: {
    pkt.opcode_name = "Zone Temperatures";
    auto payload = ramses_esp::TemperaturePayload::decode(msg.payload, msg.len);
    if (payload) {
      for (const auto &item : payload->zones) {
        pkt.fields.push_back(
            {"Zone " + std::to_string(item.zone_index) + " Temperature",
             format_float_val(item.temperature, 1) + " °C"});
      }
    }
    break;
  }
  case 0x2309: {
    pkt.opcode_name = "Zone Setpoints";
    auto payload = ramses_esp::SetpointPayload::decode(msg.payload, msg.len);
    if (payload) {
      for (const auto &item : payload->zones) {
        pkt.fields.push_back(
            {"Zone " + std::to_string(item.zone_index) + " Target Setpoint",
             format_float_val(item.setpoint, 1) + " °C"});
      }
    }
    break;
  }
  case 0x3150: {
    pkt.opcode_name = "Heat Demand";
    auto payload = ramses_esp::HeatDemandPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"Domain / Zone", std::to_string(payload->domain_or_zone_index)});
      pkt.fields.push_back(
          {"Heat Demand", format_float_val(payload->demand_percent, 1) + " %"});
    }
    break;
  }
  case 0x0004: {
    pkt.opcode_name = "Zone Name";
    auto payload = ramses_esp::ZoneNamePayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back({"Zone Index", std::to_string(payload->zone_index)});
      pkt.fields.push_back({"Zone Name", payload->name});
    }
    break;
  }
  case 0x0005: {
    pkt.opcode_name = "Zone Structure";
    auto payload =
        ramses_esp::ZoneStructurePayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back({"Zone Count", std::to_string(payload->zone_count)});
      char mask_buf[16];
      snprintf(mask_buf, sizeof(mask_buf), "0x%04X", payload->active_zone_mask);
      pkt.fields.push_back({"Active Zone Mask", mask_buf});
    }
    break;
  }
  case 0x22F1: {
    pkt.opcode_name = "HVAC Fan Preset State";
    auto payload =
        ramses_esp::FanStatePayload::decode(msg.payload, msg.len, scheme);
    if (payload) {
      pkt.fields.push_back({"Preset Mode", ramses_esp::fan_preset_to_string(
                                               payload->preset_mode)});
      pkt.fields.push_back(
          {"Fan Speed", std::to_string(payload->speed_percent) + " %"});
    }
    break;
  }
  case 0x22F3: {
    pkt.opcode_name = "HVAC Boost / Timer Control";
    auto payload = ramses_esp::FanBoostPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"Boost Duration", std::to_string(payload->minutes) + " min"});
    }
    break;
  }
  case 0x3220: {
    pkt.opcode_name = "OpenTherm Bridge Status";
    auto payload = ramses_esp::OpenThermPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back({"Flame Active", payload->flame_active
                                                ? "Active (ON)"
                                                : "Inactive (OFF)"});
      pkt.fields.push_back({"Fault Warning", payload->fault_active
                                                 ? "FAULT DETECTED"
                                                 : "Normal (OK)"});
      pkt.fields.push_back(
          {"Modulation Level",
           format_float_val(payload->modulation_percent, 1) + " %"});
      if (payload->flow_temp)
        pkt.fields.push_back(
            {"Flow Temperature",
             format_float_val(*payload->flow_temp, 1) + " °C"});
      if (payload->return_temp)
        pkt.fields.push_back(
            {"Return Temperature",
             format_float_val(*payload->return_temp, 1) + " °C"});
    }
    break;
  }
  case 0x1060: {
    pkt.opcode_name = "Device Battery & Status";
    auto payload =
        ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"Battery Level", std::to_string(payload->battery_percent) + " %"});
      pkt.fields.push_back({"Battery Low Warning",
                            payload->battery_low ? "LOW (Replace)" : "OK"});
    }
    break;
  }
  case 0x10D0: {
    pkt.opcode_name = "Filter Life Info";
    auto payload = ramses_esp::FilterInfoPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back({"Filter Remaining Days",
                            std::to_string(payload->remaining_days) + " days"});
      pkt.fields.push_back({"Filter Lifetime Days",
                            std::to_string(payload->lifetime_days) + " days"});
      pkt.fields.push_back(
          {"Filter Remaining",
           format_float_val(payload->remaining_percent, 1) + " %"});
    }
    break;
  }
  case 0x1260: {
    pkt.opcode_name = "DHW Temperature";
    auto payload =
        ramses_esp::DhwStatePayload::decode_temp(msg.payload, msg.len);
    if (payload && payload->current_temp_valid) {
      pkt.fields.push_back(
          {"DHW Current Temperature",
           format_float_val(payload->current_temp, 1) + " °C"});
    }
    break;
  }
  case 0x10A0: {
    pkt.opcode_name = "Ventilation Damper & Filter";
    auto payload =
        ramses_esp::VentilationInfoPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"Bypass Position", std::to_string(payload->bypass_position) + " %"});
      pkt.fields.push_back({"Bypass Active", payload->bypass_active
                                                 ? "Active (Open)"
                                                 : "Closed"});
      pkt.fields.push_back({"Filter Alarm", payload->filter_dirty
                                                ? "DIRTY (Replace Filter)"
                                                : "Clean"});
    }
    break;
  }
  case 0x1FC9: {
    pkt.opcode_name = "Binding / Pairing Handshake";
    auto payload = ramses_esp::BindingPayload::decode(msg);
    if (payload) {
      std::string phase =
          payload->is_offer
              ? "Offer"
              : (payload->is_accept
                     ? "Accept"
                     : (payload->is_confirm ? "Confirm" : "Binding"));
      pkt.fields.push_back({"Pairing Phase", phase});
      for (const auto &item : payload->bindings) {
        char op_buf[8];
        snprintf(op_buf, sizeof(op_buf), "%04X", item.opcode);
        pkt.fields.push_back({"Bound Device", item.address.to_string() +
                                                  " (Opcode " + op_buf + ")"});
      }
    }
    break;
  }
  case 0x2E04: {
    pkt.opcode_name = "System Operating Mode";
    auto payload = ramses_esp::SystemModePayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"System Mode", ramses_esp::system_mode_to_string(payload->mode)});
    }
    break;
  }
  case 0x12C0: {
    pkt.opcode_name = "Outdoor Temperature";
    auto payload =
        ramses_esp::OutdoorTemperaturePayload::decode(msg.payload, msg.len);
    if (payload && payload->is_valid) {
      pkt.fields.push_back({"Outdoor Temperature",
                            format_float_val(payload->temperature, 1) + " °C"});
    }
    break;
  }
  case 0x1298: {
    pkt.opcode_name = "CO2 Sensor Telemetry";
    auto payload = ramses_esp::Co2SensorPayload::decode(msg.payload, msg.len);
    if (payload && payload->is_valid) {
      pkt.fields.push_back(
          {"CO2 Level", std::to_string(payload->co2_ppm) + " ppm"});
    }
    break;
  }
  case 0x0008: {
    pkt.opcode_name = "Relay Actuator Demand";
    auto payload = ramses_esp::RelayDemandPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"Relay Index", std::to_string(payload->relay_index)});
      pkt.fields.push_back(
          {"Relay Demand",
           format_float_val(payload->demand_percent, 1) + " %"});
      pkt.fields.push_back({"Relay State", payload->is_active
                                               ? "Active (ON)"
                                               : "Inactive (OFF)"});
    }
    break;
  }
  case 0x31D9: {
    pkt.opcode_name = "HVAC Fan Info & Status";
    auto payload =
        ramses_esp::HvacFanInfoPayload::decode(msg.payload, msg.len, scheme);
    if (payload) {
      if (payload->preset_mode != ramses_esp::FanPresetMode::UNKNOWN) {
        pkt.fields.push_back({"Fan Preset", ramses_esp::fan_preset_to_string(
                                                payload->preset_mode)});
      } else {
        char mode_buf[8];
        snprintf(mode_buf, sizeof(mode_buf), "0x%02X", payload->raw_fan_mode);
        pkt.fields.push_back({"Fan Mode", mode_buf});
      }
      if (payload->fan_speed_percent) {
        pkt.fields.push_back(
            {"Fan Speed",
             format_float_val(*payload->fan_speed_percent, 0) + " %"});
      }
      if (payload->filter_dirty) {
        pkt.fields.push_back({"Filter Status", "DIRTY (Replace Filter)"});
      }
      if (payload->has_fault) {
        pkt.fields.push_back({"Fault Status", "FAULT DETECTED"});
      }
      if (payload->frost_cycle) {
        pkt.fields.push_back({"Frost Protection", "Active"});
      }
      if (payload->damper_only) {
        pkt.fields.push_back({"Damper Mode", "Damper Only"});
      }
      if (payload->passive) {
        pkt.fields.push_back({"Passive Mode", "Active"});
      }
      if (payload->bypass_position) {
        pkt.fields.push_back(
            {"Bypass Position",
             format_float_val(*payload->bypass_position, 0) + " %"});
      }
    }
    break;
  }
  case 0x1F09: {
    pkt.opcode_name = "System Sync Heartbeat";
    auto payload = ramses_esp::SystemSyncPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"System Mode", ramses_esp::system_mode_to_string(payload->mode)});
    }
    break;
  }
  case 0x000C: {
    pkt.opcode_name = "Zone Role Bindings";
    auto payload = ramses_esp::ZoneRolePayload::decode(msg.payload, msg.len);
    if (payload) {
      for (const auto &b : payload->bindings) {
        pkt.fields.push_back(
            {"Zone " + std::to_string(b.zone_index) + " Device",
             b.device_address.to_string() + " (Role " + std::to_string(b.role) +
                 ")"});
      }
    }
    break;
  }
  case 0x12A0: {
    pkt.opcode_name = "HVAC Air Quality & Sensor Array";
    auto payload = ramses_esp::AirQualityPayload::decode(msg.payload, msg.len);
    if (payload) {
      std::string loc =
          (payload->sensor_index == 0)
              ? "Indoor"
              : (payload->sensor_index == 1 ? "Supply" : "Outdoor");
      pkt.fields.push_back({"Sensor Location", loc});
      if (payload->temperature)
        pkt.fields.push_back(
            {"Temperature",
             format_float_val(*payload->temperature, 1) + " °C"});
      if (payload->humidity)
        pkt.fields.push_back(
            {"Humidity", format_float_val(*payload->humidity, 1) + " %"});
    }
    break;
  }
  case 0x12B0: {
    pkt.opcode_name = "Window / Door Contact Sensor";
    auto payload =
        ramses_esp::ContactSensorPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"Contact State", payload->is_open ? "OPEN" : "CLOSED"});
    }
    break;
  }
  case 0x3EF0:
  case 0x3EF1:
  case 0x3B00: {
    pkt.opcode_name = "Actuator Modulation & Relay State";
    auto payload =
        ramses_esp::ActuatorStatePayload::decode(msg.payload, msg.len, opcode);
    if (payload) {
      pkt.fields.push_back(
          {"Modulation Level",
           format_float_val(payload->modulation_percent, 1) + " %"});
      pkt.fields.push_back({"Relay State", payload->relay_active
                                               ? "Active (ON)"
                                               : "Inactive (OFF)"});
    }
    break;
  }
  case 0x0418:
  case 0x042F:
  case 0x4401: {
    pkt.opcode_name = "System Fault Log";
    auto payload =
        ramses_esp::SystemFaultLogPayload::decode(msg.payload, msg.len, opcode);
    if (payload) {
      char f_buf[8];
      snprintf(f_buf, sizeof(f_buf), "0x%02X", payload->fault_code);
      pkt.fields.push_back({"Fault Code", f_buf});
      pkt.fields.push_back({"Fault State", payload->is_fault
                                               ? "FAULT ACTIVE"
                                               : "Restored (OK)"});
    }
    break;
  }
  case 0x4E01:
  case 0x4E02: {
    pkt.opcode_name = "Spider / Autotemp Thermostat";
    auto payload =
        ramses_esp::SpiderTemperaturesPayload::decode(msg.payload, msg.len);
    if (payload && payload->primary_temp) {
      pkt.fields.push_back(
          {"Temperature", format_float_val(*payload->primary_temp, 1) + " °C"});
    }
    break;
  }
  case 0x12F0: {
    pkt.opcode_name = "DHW Flow Rate";
    auto payload = ramses_esp::DhwConfigPayload::decode(msg.payload, msg.len);
    if (payload) {
      pkt.fields.push_back(
          {"DHW Flow Rate",
           format_float_val(payload->flow_rate, 1) + " L/min"});
    }
    break;
  }
  case 0x313F: {
    pkt.opcode_name = "System Clock Synchronization";
    break;
  }
  case 0x31E0: {
    pkt.opcode_name = "HVAC Ventilation Demand";
    break;
  }
  default: {
    pkt.opcode_name = "RAMSES Opcode " + pkt.opcode_hex;
    break;
  }
  }
}

static std::string json_escape(const std::string &input) {
  std::string output;
  output.reserve(input.size() + 16);
  for (char c : input) {
    switch (c) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\b':
      output += "\\b";
      break;
    case '\f':
      output += "\\f";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
        output += buf;
      } else {
        output += c;
      }
      break;
    }
  }
  return output;
}

#ifdef RAMSES_HAS_WEB_SERVER_BASE
class RamsesWebHandler : public AsyncWebHandler {
public:
  explicit RamsesWebHandler(RamsesDiscoveryComponent *parent)
      : parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) const override {
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    StringRef u = request->url_to(url_buf);
    return u == "/ramses" || u == "/discovery" || u == "/ramses/devices.json" ||
           u == "/ramses/probe" || u == "/ramses/api/config" ||
           u == "/ramses/api/reset";
  }

  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                  size_t index, size_t total) override {
    if (index == 0) {
      this->body_buffer_.clear();
      this->body_buffer_.reserve(total);
    }
    this->body_buffer_.append(reinterpret_cast<const char *>(data), len);
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    StringRef u = request->url_to(url_buf);
    if (u == "/ramses" || u == "/discovery") {
      const uint8_t *html_gz = this->parent_->get_html_gz();
      size_t html_gz_len = this->parent_->get_html_gz_len();
      if (html_gz != nullptr && html_gz_len > 0) {
#ifndef USE_ESP8266
        AsyncWebServerResponse *response =
            request->beginResponse(200, "text/html", html_gz, html_gz_len);
#else
        AsyncWebServerResponse *response =
            request->beginResponse_P(200, "text/html", html_gz, html_gz_len);
#endif
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      } else {
        const char *html = this->parent_->get_html();
        if (html == nullptr) {
          html = RAMSES_DISCOVERY_FALLBACK_HTML;
        }
        request->send(200, "text/html", html);
      }
    } else if (u == "/ramses/devices.json") {
      uint32_t now = 0;
#ifdef USE_ESP_IDF
      now = millis();
#endif
      std::string json = this->parent_->generate_json(now);
      request->send(200, "application/json", json.c_str());
    } else if (u == "/ramses/probe") {
      this->parent_->trigger_probe();
      request->send(200, "application/json",
                    "{\"status\":\"probing_triggered\"}");
    } else if (u == "/ramses/api/config") {
      if (request->method() == HTTP_GET) {
        std::string cfg = RamsesNvsStorage::instance().load_config();
        if (cfg.empty()) {
          cfg = "{\"configured\":false,\"devices\":[]}";
        }
        request->send(200, "application/json", cfg.c_str());
      } else {
        bool ok = RamsesNvsStorage::instance().save_config(this->body_buffer_);
        if (ok) {
          request->send(200, "application/json",
                        "{\"status\":\"ok\",\"message\":\"Configuration saved. "
                        "Applying to Home Assistant...\",\"rebooting\":true}");
          this->parent_->schedule_reboot(600);
        } else {
          request->send(500, "application/json",
                        "{\"status\":\"error\",\"message\":\"Failed to save "
                        "configuration.\"}");
        }
      }
    } else if (u == "/ramses/api/reset") {
      RamsesNvsStorage::instance().clear_config();
      request->send(200, "application/json",
                    "{\"status\":\"reset\",\"message\":\"Configuration reset. "
                    "Restarting in discovery mode...\",\"rebooting\":true}");
      this->parent_->schedule_reboot(600);
    }
  }

protected:
  RamsesDiscoveryComponent *parent_;
  std::string body_buffer_;
};
#endif

void RamsesDiscoveryComponent::setup() {
  ESP_LOGI(TAG, "Initializing RAMSES Auto-Discovery engine...");

  // Load active Home Assistant entities from stored configuration (if present)
  size_t loaded_count =
      RamsesNvsStorage::instance().load_and_register_entities(this->parent_);
  if (loaded_count > 0) {
    ESP_LOGI(TAG,
             "Loaded %u active Home Assistant entity/entities from stored "
             "configuration",
             (unsigned)loaded_count);
  }

#ifdef USE_ESP_IDF
  if (this->parent_ != nullptr) {
    this->parent_->add_raw_message_callback(
        [this](const ramses_esp::RamsesMessage &msg) {
          this->on_message(msg);
        });
  }
#endif

#ifdef RAMSES_HAS_WEB_SERVER_BASE
  if (this->web_server_base_ == nullptr &&
      web_server_base::global_web_server_base != nullptr) {
    this->web_server_base_ = web_server_base::global_web_server_base;
  }
  if (this->web_server_base_ != nullptr) {
    this->web_server_base_->add_handler(new RamsesWebHandler(this));
    ESP_LOGI(TAG, "Registered RAMSES Discovery Web UI dashboard at /ramses");
  }
#endif
}

void RamsesDiscoveryComponent::schedule_reboot(uint32_t delay_ms) {
#ifdef USE_ESP_IDF
  this->set_timeout(delay_ms, []() { App.safe_reboot(); });
#endif
}

bool RamsesDiscoveryComponent::is_nvs_configured() const {
  return RamsesNvsStorage::instance().is_configured();
}

void RamsesDiscoveryComponent::loop() {
  uint32_t now = millis();
  if (this->active_probing_ &&
      (now - this->last_probe_time_ > this->probing_interval_ms_)) {
    this->last_probe_time_ = now;
    this->probe_pending();
  }

  // Drive loop() on dynamically instantiated entities (fan, etc.) that are
  // registered as entities but not as ESPHome looping components (since
  // App.register_component_() is protected and we cannot call it at runtime).
  for (Component *comp :
       RamsesNvsStorage::instance().get_dynamic_components()) {
    comp->loop();
  }

  // Periodic discovery summary dump every 60s (brief status log)
  if (now - this->last_dump_time_ > 60000) {
    this->last_dump_time_ = now;
    if (!this->devices_.empty()) {
      ESP_LOGI(
          TAG,
          "Discovery active: %d device(s) detected. Web dashboard at /ramses",
          (int)this->devices_.size());
    }
  }
}

void RamsesDiscoveryComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RAMSES Discovery Component:");
  ESP_LOGCONFIG(TAG, "  Active Probing: %s",
                this->active_probing_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Probing Interval: %u ms",
                (unsigned int)this->probing_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Discovered Devices: %d", (int)this->devices_.size());
}

DiscoveredDevice &RamsesDiscoveryComponent::get_or_create_device(
    const ramses_esp::RamsesAddress &addr) {
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
    dev.hvac_scheme = ramses_esp::HvacScheme::HOPPER;
    dev.oem_name = "hopper";
    break;
  default:
    dev.device_type = "other";
    break;
  }

  this->devices_[key] = dev;
  return this->devices_[key];
}

void RamsesDiscoveryComponent::on_message(
    const ramses_esp::RamsesMessage &msg) {
  // 1. Process Source Device (addr[0])
  if (msg.fields & RAMSES_F_ADDR0) {
    ramses_esp::RamsesAddress src =
        ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
    if (src.is_valid && src.dev_class != 63) {
      DiscoveredDevice &dev = this->get_or_create_device(src);
      dev.last_rssi = msg.rssi;
#ifdef USE_ESP_IDF
      dev.last_seen_ms = millis();
#else
      dev.last_seen_ms = 1000;
#endif
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
  }

  // 2. Also register Target Device (addr[1]) if present (ignore broadcast and
  // gateways)
  if (msg.fields & RAMSES_F_ADDR1) {
    ramses_esp::RamsesAddress dst =
        ramses_esp::RamsesAddress::from_bytes(msg.addr[1]);
    if (dst.is_valid && dst.dev_class != 63 && dst.dev_class != 18) {
      DiscoveredDevice &dst_dev = this->get_or_create_device(dst);
#ifdef USE_ESP_IDF
      dst_dev.last_seen_ms = millis();
#else
      dst_dev.last_seen_ms = 1000;
#endif
      uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
      dst_dev.seen_opcodes.insert(opcode);

      // If fan command 22F1/22F3 was sent to target, target is the HVAC unit
      if (opcode == 0x22F1 || opcode == 0x22F3) {
        dst_dev.is_hvac = true;
        dst_dev.device_type = "hvac";
        if (msg.fields & RAMSES_F_ADDR0) {
          ramses_esp::RamsesAddress src =
              ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
          if (src.is_valid && src.dev_class != 63 && src.dev_class != 18) {
            dst_dev.associated_remote = src.to_string();
            DiscoveredDevice &src_dev = this->get_or_create_device(src);
            src_dev.device_type = "remote";
            src_dev.is_hvac = false;
            src_dev.associated_target = dst.to_string();

            // Propagate HVAC scheme between paired remote and fan
            if (dst_dev.hvac_scheme != ramses_esp::HvacScheme::AUTO &&
                src_dev.hvac_scheme == ramses_esp::HvacScheme::AUTO) {
              src_dev.hvac_scheme = dst_dev.hvac_scheme;
              src_dev.oem_name = dst_dev.oem_name;
            } else if (src_dev.hvac_scheme != ramses_esp::HvacScheme::AUTO &&
                       dst_dev.hvac_scheme == ramses_esp::HvacScheme::AUTO) {
              dst_dev.hvac_scheme = src_dev.hvac_scheme;
              dst_dev.oem_name = src_dev.oem_name;
            }
          }
        }
        this->process_hvac_packet(dst_dev, msg, opcode);
      }
    }
  }

  // Record packet for live web traffic inspector after updating device schemes
  DiscoveredPacket pkt;
  pkt.id = this->packet_seq_id_++;
#ifdef USE_ESP_IDF
  pkt.timestamp_ms = millis();
#else
  pkt.timestamp_ms = 1000;
#endif
  pkt.rssi = msg.rssi;
  pkt.hgi80 = msg.to_hgi80();
  this->decode_packet_details(msg, pkt);
  this->recent_packets_.push_front(pkt);
  while (this->recent_packets_.size() > MAX_RECENT_PACKETS) {
    this->recent_packets_.pop_back();
  }
}

void RamsesDiscoveryComponent::process_controller_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x30C9) {
    auto dec =
        ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
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
               dec->zone_index, dec->name.c_str(),
               dev.address.to_string().c_str());
    }
  } else if (opcode == 0x1260 || opcode == 0x12F0 || opcode == 0x1F41) {
    dev.has_dhw = true;
  }
}

void RamsesDiscoveryComponent::process_hvac_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x10E0) {
    auto dec =
        ramses_esp::DeviceInfoPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.oem_probed = true;
      if (dec->oem_code == 0x67) {
        dev.oem_name = "orcon";
        dev.hvac_scheme = ramses_esp::HvacScheme::ORCON;
      } else if (dec->oem_code == 0x6A) {
        dev.oem_name = "hopper";
        dev.hvac_scheme = ramses_esp::HvacScheme::HOPPER;
      } else if (dec->oem_code == 0x08 || dec->oem_code == 0x01) {
        dev.oem_name = "itho";
        dev.hvac_scheme = ramses_esp::HvacScheme::ITHO;
      } else if (dec->oem_code == 0x13 || dec->oem_code == 0x66) {
        dev.oem_name = "vasco";
        dev.hvac_scheme = ramses_esp::HvacScheme::VASCO;
      } else if (dec->oem_code == 0x02) {
        dev.oem_name = "zehnder";
        dev.hvac_scheme = ramses_esp::HvacScheme::ZEHNDER;
      }
      ESP_LOGI(TAG, "Identified HVAC Unit %s OEM Scheme: %s",
               dev.address.to_string().c_str(), dev.oem_name.c_str());
    }
  } else if (opcode == 0x1FC9) {
    auto dec = ramses_esp::BindingPayload::decode(msg);
    if (dec.has_value()) {
      for (const auto &item : dec->bindings) {
        if (item.oem_code == 0x6A) {
          dev.oem_name = "hopper";
          dev.hvac_scheme = ramses_esp::HvacScheme::HOPPER;
          dev.oem_probed = true;
          break;
        } else if (item.oem_code == 0x67) {
          dev.oem_name = "orcon";
          dev.hvac_scheme = ramses_esp::HvacScheme::ORCON;
          dev.oem_probed = true;
          break;
        } else if (item.oem_code == 0x13 || item.oem_code == 0x66) {
          dev.oem_name = "vasco";
          dev.hvac_scheme = ramses_esp::HvacScheme::VASCO;
          dev.oem_probed = true;
          break;
        } else if (item.oem_code == 0x08 || item.oem_code == 0x01) {
          dev.oem_name = "itho";
          dev.hvac_scheme = ramses_esp::HvacScheme::ITHO;
          dev.oem_probed = true;
          break;
        }
      }
    }
  } else if (opcode == 0x10D0) {
    auto dec =
        ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["filter_remaining_days"] =
          static_cast<float>(dec->remaining_days);
    }
  } else if (opcode == 0x22F1) {
    auto dec = ramses_esp::FanStatePayload::decode(msg.payload, msg.n_payload,
                                                   dev.hvac_scheme);
    if (dec.has_value()) {
      dev.last_telemetry["fan_speed"] = static_cast<float>(dec->speed_percent);
      ESP_LOGI(TAG, "HVAC Unit %s Fan Speed: %u%% (Preset: %s)",
               dev.address.to_string().c_str(), (unsigned)dec->speed_percent,
               ramses_esp::fan_preset_to_string(dec->preset_mode));
    }
  } else if (opcode == 0x22F3) {
    auto dec = ramses_esp::FanBoostPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["fan_speed"] = 100.0f;
      dev.last_telemetry["timer_minutes"] = static_cast<float>(dec->minutes);
      ESP_LOGI(TAG, "HVAC Unit %s Fan Boost Timer: %u min (Speed: 100%%)",
               dev.address.to_string().c_str(), (unsigned)dec->minutes);
    }
  } else if (opcode == 0x31D9) {
    auto dec = ramses_esp::HvacFanInfoPayload::decode(
        msg.payload, msg.n_payload, dev.hvac_scheme);
    if (dec.has_value()) {
      if (dec->fan_speed_percent.has_value()) {
        dev.last_telemetry["fan_speed"] = *dec->fan_speed_percent;
      }
      if (dec->bypass_position.has_value()) {
        dev.last_telemetry["bypass_position"] = *dec->bypass_position;
      }
      dev.last_telemetry["filter_dirty"] = dec->filter_dirty ? 1.0f : 0.0f;
      dev.last_telemetry["fault"] = dec->has_fault ? 1.0f : 0.0f;
      ESP_LOGI(TAG, "HVAC Unit %s Fan Info: Preset=%s (Filter=%s, Fault=%s)",
               dev.address.to_string().c_str(),
               ramses_esp::fan_preset_to_string(dec->preset_mode),
               dec->filter_dirty ? "DIRTY" : "OK",
               dec->has_fault ? "FAULT" : "OK");
    }
  } else if (opcode == 0x31DA) {
    auto dec =
        ramses_esp::HvacTelemetryPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      if (dec->supply_temp.has_value())
        dev.last_telemetry["supply_temperature"] = *dec->supply_temp;
      if (dec->exhaust_temp.has_value())
        dev.last_telemetry["exhaust_temperature"] = *dec->exhaust_temp;
      if (dec->indoor_temp.has_value())
        dev.last_telemetry["indoor_temperature"] = *dec->indoor_temp;
      if (dec->outdoor_temp.has_value())
        dev.last_telemetry["outdoor_temperature"] = *dec->outdoor_temp;
      if (dec->bypass_position.has_value())
        dev.last_telemetry["bypass_position"] = *dec->bypass_position;
      if (dec->supply_fan_speed.has_value())
        dev.last_telemetry["supply_fan_speed"] = *dec->supply_fan_speed;
      if (dec->exhaust_fan_speed.has_value())
        dev.last_telemetry["exhaust_fan_speed"] = *dec->exhaust_fan_speed;
      if (dec->indoor_humidity.has_value())
        dev.last_telemetry["indoor_humidity"] = *dec->indoor_humidity;
      if (dec->outdoor_humidity.has_value())
        dev.last_telemetry["outdoor_humidity"] = *dec->outdoor_humidity;
      if (dec->co2_ppm.has_value())
        dev.last_telemetry["co2"] = static_cast<float>(*dec->co2_ppm);
      if (dec->remaining_mins.has_value())
        dev.last_telemetry["timer_minutes"] =
            static_cast<float>(*dec->remaining_mins);
      ESP_LOGI(TAG,
               "HVAC Unit %s Telemetry: Supply=%.1f°C, Exhaust=%.1f°C, "
               "Indoor=%.1f°C, Bypass=%.0f%%",
               dev.address.to_string().c_str(), dec->supply_temp.value_or(0.0f),
               dec->exhaust_temp.value_or(0.0f),
               dec->indoor_temp.value_or(0.0f),
               dec->bypass_position.value_or(0.0f));
    }
  } else if (opcode == 0x1298) {
    auto dec = ramses_esp::Co2SensorPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["co2"] = static_cast<float>(dec->co2_ppm);
    }
  } else if (opcode == 0x12A0) {
    auto dec =
        ramses_esp::AirQualityPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      if (dec->temperature.has_value())
        dev.last_telemetry["indoor_temperature"] = *dec->temperature;
      if (dec->humidity.has_value())
        dev.last_telemetry["indoor_humidity"] = *dec->humidity;
    }
  }
}

void RamsesDiscoveryComponent::process_trv_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x3150) {
    auto dec =
        ramses_esp::HeatDemandPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["heat_demand"] = dec->demand_percent;
    }
  } else if (opcode == 0x1060) {
    auto dec =
        ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["battery_level"] = dec->battery_percent;
    }
  }
}

void RamsesDiscoveryComponent::process_opentherm_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x3220) {
    auto dec = ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["modulation"] = dec->modulation_percent;
      if (dec->flow_temp.has_value())
        dev.last_telemetry["flow_temperature"] = *dec->flow_temp;
      if (dec->return_temp.has_value())
        dev.last_telemetry["return_temperature"] = *dec->return_temp;
      dev.last_telemetry["flame_active"] = dec->flame_active ? 1.0f : 0.0f;
      if (dec->fault_active)
        dev.last_telemetry["fault"] = 1.0f;
    }
  }
}

void RamsesDiscoveryComponent::process_sensor_packet(
    DiscoveredDevice &dev, const ramses_esp::RamsesMessage &msg,
    uint16_t opcode) {
  if (opcode == 0x12C0) {
    auto dec = ramses_esp::OutdoorTemperaturePayload::decode(msg.payload,
                                                             msg.n_payload);
    if (dec.has_value() && dec->is_valid) {
      dev.last_telemetry["outdoor_temperature"] = dec->temperature;
    }
  } else if (opcode == 0x1060) {
    auto dec =
        ramses_esp::DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["battery_level"] = dec->battery_percent;
      ESP_LOGI(TAG, "Device %s Battery: %u%% (Low: %s)",
               dev.address.to_string().c_str(), (unsigned)dec->battery_percent,
               dec->battery_low ? "YES" : "NO");
    }
  } else if (opcode == 0x12B0) {
    auto dec =
        ramses_esp::WindowStatePayload::decode(msg.payload, msg.n_payload);
    if (dec.has_value()) {
      dev.last_telemetry["window_open"] = dec->window_open ? 1.0f : 0.0f;
    }
  } else if (opcode == 0x22C9 || opcode == 0x2209) {
    auto dec = ramses_esp::UfhSetpointBoundsPayload::decode(msg.payload,
                                                            msg.n_payload);
    if (dec.has_value()) {
      if (dec->min_temp.has_value())
        dev.last_telemetry["ufh_min_temp"] = *dec->min_temp;
      if (dec->max_temp.has_value())
        dev.last_telemetry["ufh_max_temp"] = *dec->max_temp;
    }
  } else if (opcode == 0x3EF0 || opcode == 0x3EF1 || opcode == 0x3B00) {
    auto dec = ramses_esp::ActuatorStatePayload::decode(msg.payload,
                                                        msg.n_payload, opcode);
    if (dec.has_value()) {
      dev.last_telemetry["actuator_modulation"] = dec->modulation_percent;
    }
  } else if (opcode == 0x0418 || opcode == 0x0009 || opcode == 0x4401) {
    auto dec = ramses_esp::SystemFaultLogPayload::decode(msg.payload,
                                                         msg.n_payload, opcode);
    if (dec.has_value()) {
      dev.last_telemetry["fault_code"] = static_cast<float>(dec->fault_code);
    }
  } else if (opcode == 0x4E01 || opcode == 0x4E02) {
    auto dec = ramses_esp::SpiderTemperaturesPayload::decode(msg.payload,
                                                             msg.n_payload);
    if (dec.has_value() && dec->primary_temp.has_value()) {
      dev.last_telemetry["spider_temperature"] = *dec->primary_temp;
    }
  }
}

void RamsesDiscoveryComponent::trigger_probe() {
  ESP_LOGI(
      TAG,
      "Manual probe triggered: resetting probe states and querying network");
  for (auto &kv : this->devices_) {
    kv.second.oem_probed = false;
    for (auto &zkv : kv.second.zones) {
      zkv.second.name_probed = false;
    }
  }
  this->probe_pending();
}

void RamsesDiscoveryComponent::probe_pending() {
#ifdef USE_ESP_IDF
  if (this->parent_ == nullptr)
    return;

  ramses_esp::RamsesAddress hgi_src{
      .dev_class = 18, .id = 0x005612, .is_valid = true};

  if (this->devices_.empty()) {
    ESP_LOGI(
        TAG,
        "No devices discovered yet; broadcasting discovery query (RQ 10E0)...");
    ramses_esp::RamsesAddress bcast{
        .dev_class = 63, .id = 0x3FFFFF, .is_valid = true};
    ramses_esp::RamsesMessage query =
        ramses_esp::DeviceInfoPayload::encode_query(hgi_src, bcast);
    this->parent_->send_message(query);
    return;
  }

  for (auto &kv : this->devices_) {
    DiscoveredDevice &dev = kv.second;

    // Probe un-named zones on controllers
    if (dev.device_type == "controller") {
      for (auto &zkv : dev.zones) {
        DiscoveredZone &zone = zkv.second;
        if (zone.name.empty() && !zone.name_probed) {
          zone.name_probed = true;
          ESP_LOGI(TAG, "Probing zone %d name on controller %s (RQ 0004)...",
                   (int)zone.index, dev.address.to_string().c_str());
          ramses_esp::RamsesMessage query =
              ramses_esp::ZoneNamePayload::encode_query(hgi_src, dev.address,
                                                        zone.index);
          this->parent_->send_message(query);
          return; // Send one probe per loop to avoid RF collisions
        }
      }
    }

    // Probe OEM signature and Status on HVAC units
    if (dev.is_hvac && !dev.oem_probed) {
      dev.oem_probed = true;
      ESP_LOGI(TAG, "Probing HVAC unit %s for Device Info (RQ 10E0)...",
               dev.address.to_string().c_str());
      ramses_esp::RamsesMessage query_info =
          ramses_esp::DeviceInfoPayload::encode_query(hgi_src, dev.address);
      this->parent_->send_message(query_info);

      ESP_LOGI(TAG, "Probing HVAC unit %s for Status (RQ 31DA)...",
               dev.address.to_string().c_str());
      ramses_esp::RamsesMessage query_status =
          ramses_esp::RamsesMessageBuilder::query()
              .from(hgi_src)
              .to(dev.address)
              .opcode(0x31DA)
              .payload({0x00})
              .build();
      this->parent_->send_message(query_status);
      return;
    }
  }
#endif
}

static std::string sanitize_device_id(const ramses_esp::RamsesAddress &addr) {
  std::string s = addr.to_string();
  for (char &c : s) {
    if (c == ':') {
      c = '_';
    }
  }
  return "ramses_" + s;
}

static std::string get_device_title(const DiscoveredDevice &dev) {
  std::string addr_str = dev.address.to_string();
  if (dev.device_type == "controller") {
    return "Evohome Controller " + addr_str;
  }
  if (dev.is_hvac || dev.device_type == "hvac") {
    return "Ventilation Unit " + addr_str;
  }
  if (dev.device_type == "trv") {
    return "Radiator TRV " + addr_str;
  }
  if (dev.device_type == "opentherm") {
    return "OpenTherm Bridge " + addr_str;
  }
  if (dev.device_type == "sensor") {
    return "Room Sensor " + addr_str;
  }
  if (dev.device_type == "relay") {
    return "BDR91 Relay " + addr_str;
  }
  return "RAMSES Device " + addr_str;
}

static bool device_has_entities(const DiscoveredDevice &dev) {
  if (dev.device_type == "controller") {
    return !dev.zones.empty() || dev.has_dhw;
  }
  if (dev.is_hvac || dev.device_type == "hvac") {
    return true;
  }
  if (dev.device_type == "trv") {
    return true;
  }
  if (dev.device_type == "opentherm") {
    return true;
  }
  return false;
}

std::string RamsesDiscoveryComponent::generate_device_yaml(
    const DiscoveredDevice &dev) const {
  std::stringstream ss;
  std::string addr_str = dev.address.to_string();
  std::string dev_id = sanitize_device_id(dev.address);

  if (dev.device_type == "controller") {
    if (!dev.zones.empty()) {
      ss << "climate:\n";
      for (const auto &zkv : dev.zones) {
        const DiscoveredZone &z = zkv.second;
        std::string zone_name =
            z.name.empty() ? ("Zone " + std::to_string(z.index)) : z.name;
        ss << "  - platform: ramses_devices\n";
        ss << "    ramses_esp_id: ramses_hub\n";
        ss << "    name: \"" << zone_name << " Heating\"\n";
        ss << "    controller_address: \"" << addr_str << "\"\n";
        ss << "    zone_index: " << (int)z.index << "\n";
        ss << "    zone_name: \"" << zone_name << "\"\n";
        ss << "    device_id: " << dev_id << "\n\n";
      }
    }
    if (dev.has_dhw) {
      ss << "water_heater:\n";
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Domestic Hot Water\"\n";
      ss << "    controller_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    }
  } else if (dev.is_hvac) {
    ss << "fan:\n";
    ss << "  - platform: ramses_devices\n";
    ss << "    ramses_esp_id: ramses_hub\n";
    ss << "    name: \"Ventilation Unit\"\n";
    ss << "    device_address: \"" << addr_str << "\"\n";
    if (!dev.associated_remote.empty()) {
      ss << "    # fake_remote_address: \"" << dev.associated_remote
         << "\"  # Optional: uncomment if cloning this remote\n";
    }
    ss << "    scheme: " << dev.oem_name << "\n";
    ss << "    device_id: " << dev_id << "\n\n";

    bool has_sensor = false;
    auto emit_sensor = [&](const std::string &type, const std::string &name) {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.last_telemetry.count("supply_temperature"))
      emit_sensor("supply_temperature", "Ventilation Supply Temperature");
    if (dev.last_telemetry.count("exhaust_temperature"))
      emit_sensor("exhaust_temperature", "Ventilation Exhaust Temperature");
    if (dev.last_telemetry.count("indoor_temperature") ||
        dev.last_telemetry.count("air_quality_temperature"))
      emit_sensor("air_quality_temperature", "Ventilation Indoor Temperature");
    if (dev.last_telemetry.count("outdoor_temperature"))
      emit_sensor("outdoor_temperature", "Ventilation Outdoor Temperature");
    if (dev.last_telemetry.count("bypass_position"))
      emit_sensor("bypass_position", "Ventilation Bypass Position");
    if (dev.last_telemetry.count("supply_fan_speed"))
      emit_sensor("supply_fan_speed", "Ventilation Supply Fan Speed");
    if (dev.last_telemetry.count("exhaust_fan_speed"))
      emit_sensor("exhaust_fan_speed", "Ventilation Exhaust Fan Speed");
    if (dev.last_telemetry.count("timer_minutes") ||
        dev.last_telemetry.count("remaining_mins"))
      emit_sensor("remaining_mins", "Ventilation Remaining Timer");
    if (dev.last_telemetry.count("filter_remaining_days"))
      emit_sensor("filter_remaining_days", "Ventilation Filter Remaining Days");
    if (dev.last_telemetry.count("filter_lifetime_days"))
      emit_sensor("filter_lifetime_days", "Ventilation Filter Lifetime");
    if (dev.last_telemetry.count("filter_remaining_percent"))
      emit_sensor("filter_remaining_percent",
                  "Ventilation Filter Remaining Percent");
    if (dev.last_telemetry.count("indoor_humidity"))
      emit_sensor("indoor_humidity", "Ventilation Indoor Humidity");
    if (dev.last_telemetry.count("outdoor_humidity"))
      emit_sensor("outdoor_humidity", "Ventilation Outdoor Humidity");
    if (dev.last_telemetry.count("co2"))
      emit_sensor("co2", "Ventilation CO2 Level");

    bool has_bin = false;
    auto emit_bin = [&](const std::string &type, const std::string &name) {
      if (!has_bin) {
        ss << "binary_sensor:\n";
        has_bin = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.last_telemetry.count("filter_dirty"))
      emit_bin("filter_alarm", "Ventilation Filter Dirty Warning");
    if (dev.last_telemetry.count("bypass_active"))
      emit_bin("bypass_active", "Ventilation Bypass Active");

  } else if (dev.device_type == "remote") {
    ss << "# Discovered RF Remote Control: " << addr_str << "\n";
    if (!dev.associated_target.empty()) {
      ss << "# Paired to MVHR Unit: " << dev.associated_target << "\n";
      ss << "# (The fan configuration already clones this remote using "
            "fake_remote_address: \""
         << addr_str << "\")\n\n";
    }
  } else if (dev.device_type == "opentherm") {
    bool has_sensor = false;
    auto emit_sensor = [&](const std::string &type, const std::string &name) {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.last_telemetry.count("modulation"))
      emit_sensor("opentherm_modulation", "Boiler Modulation");
    if (dev.last_telemetry.count("flow_temperature"))
      emit_sensor("flow_temperature", "Boiler Flow Temperature");
    if (dev.last_telemetry.count("return_temperature"))
      emit_sensor("return_temperature", "Boiler Return Temperature");

    bool has_bin = false;
    auto emit_bin = [&](const std::string &type, const std::string &name) {
      if (!has_bin) {
        ss << "binary_sensor:\n";
        has_bin = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.last_telemetry.count("flame_active"))
      emit_bin("flame_active", "Boiler Flame Active");
    if (dev.last_telemetry.count("fault") ||
        dev.last_telemetry.count("fault_code"))
      emit_bin("fault_alarm", "Boiler Fault Warning");

  } else if (dev.device_type == "trv") {
    bool has_sensor = false;
    auto emit_sensor = [&](const std::string &type, const std::string &name) {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.last_telemetry.count("heat_demand") ||
        dev.seen_opcodes.count(0x3150))
      emit_sensor("heat_demand", "TRV " + addr_str + " Heat Demand");
    if (dev.last_telemetry.count("battery_level") ||
        dev.seen_opcodes.count(0x1060))
      emit_sensor("battery_level", "TRV " + addr_str + " Battery Level");

    if (dev.last_telemetry.count("battery_low") ||
        dev.last_telemetry.count("battery_level") ||
        dev.seen_opcodes.count(0x1060)) {
      ss << "binary_sensor:\n";
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: battery_low\n";
      ss << "    name: \"TRV " << addr_str << " Battery Low Warning\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    }
  } else if (dev.device_type == "relay") {
    if (dev.last_telemetry.count("relay_demand") ||
        dev.seen_opcodes.count(0x0008)) {
      ss << "sensor:\n";
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: relay_demand\n";
      ss << "    name: \"Relay " << addr_str << " Demand\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    relay_index: 0\n";
      ss << "    device_id: " << dev_id << "\n\n";
    }
  } else if (dev.device_type == "sensor") {
    bool has_sensor = false;
    auto emit_sensor = [&](const std::string &type, const std::string &name) {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.last_telemetry.count("outdoor_temperature"))
      emit_sensor("outdoor_temperature",
                  "Sensor " + addr_str + " Outdoor Temperature");
    if (dev.last_telemetry.count("indoor_temperature"))
      emit_sensor("air_quality_temperature",
                  "Sensor " + addr_str + " Indoor Temperature");
    if (dev.last_telemetry.count("indoor_humidity"))
      emit_sensor("indoor_humidity", "Sensor " + addr_str + " Indoor Humidity");
    if (dev.last_telemetry.count("co2"))
      emit_sensor("co2", "Sensor " + addr_str + " CO2 Level");
  }

  return ss.str();
}

std::string RamsesDiscoveryComponent::generate_json(uint32_t now_ms) const {
  std::stringstream ss;
  ss << "{\n";
  ss << "  \"device_count\": " << this->devices_.size() << ",\n";
  ss << "  \"is_configured\": "
     << (RamsesNvsStorage::instance().is_configured() ? "true" : "false")
     << ",\n";
  ss << "  \"configured_entities\": "
     << RamsesNvsStorage::instance().get_entity_count() << ",\n";

  const auto &configured_addrs =
      RamsesNvsStorage::instance().get_configured_addresses();
  ss << "  \"configured_addresses\": [";
  bool first_ca = true;
  for (const auto &ca : configured_addrs) {
    if (!first_ca)
      ss << ", ";
    first_ca = false;
    ss << "\"" << json_escape(ca) << "\"";
  }
  ss << "],\n";

  ss << "  \"devices\": [\n";

  bool first_dev = true;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (!first_dev) {
      ss << ",\n";
    }
    first_dev = false;

    uint32_t last_seen_ago_sec = 0;
    if (now_ms > 0 && dev.last_seen_ms > 0 && now_ms >= dev.last_seen_ms) {
      last_seen_ago_sec = (now_ms - dev.last_seen_ms) / 1000;
    }

    std::string type_label = dev.device_type;
    if (dev.device_type == "controller") {
      type_label = "Evohome Controller";
    } else if (dev.device_type == "trv") {
      type_label = "Radiator TRV (HR92)";
    } else if (dev.device_type == "opentherm") {
      type_label = "OpenTherm Bridge (R8810)";
    } else if (dev.device_type == "remote") {
      type_label = "HVAC Remote Control";
      if (!dev.associated_target.empty()) {
        type_label += " (Paired to " + dev.associated_target + ")";
      }
    } else if (dev.is_hvac) {
      type_label = "MVHR Fan (" + dev.oem_name + ")";
    } else if (dev.device_type == "sensor") {
      type_label = "Room Sensor / Thermostat";
    } else if (dev.device_type == "gateway") {
      type_label = "HGI80 / RF Gateway";
    } else if (dev.device_type == "relay") {
      type_label = "BDR91 Relay";
    }

    bool is_saved = RamsesNvsStorage::instance().is_device_configured(
        dev.address.to_string());

    ss << "    {\n";
    ss << "      \"address\": \"" << dev.address.to_string() << "\",\n";
    ss << "      \"device_type\": \"" << json_escape(dev.device_type)
       << "\",\n";
    ss << "      \"type_label\": \"" << json_escape(type_label) << "\",\n";
    ss << "      \"oem_name\": \"" << json_escape(dev.oem_name) << "\",\n";
    ss << "      \"associated_remote\": \""
       << json_escape(dev.associated_remote) << "\",\n";
    ss << "      \"associated_target\": \""
       << json_escape(dev.associated_target) << "\",\n";
    ss << "      \"rssi\": " << (int)dev.last_rssi << ",\n";
    ss << "      \"last_seen_sec\": " << last_seen_ago_sec << ",\n";
    ss << "      \"has_dhw\": " << (dev.has_dhw ? "true" : "false") << ",\n";
    ss << "      \"is_saved\": " << (is_saved ? "true" : "false") << ",\n";

    // Zones
    ss << "      \"zones\": [";
    bool first_zone = true;
    for (const auto &zkv : dev.zones) {
      const DiscoveredZone &z = zkv.second;
      if (!first_zone)
        ss << ", ";
      first_zone = false;
      ss << "{\"index\": " << (int)z.index;
      ss << ", \"name\": \"" << json_escape(z.name) << "\"";
      if (z.has_temp)
        ss << ", \"temp\": " << std::fixed << std::setprecision(1)
           << z.last_temp;
      if (z.has_setpoint)
        ss << ", \"setpoint\": " << std::fixed << std::setprecision(1)
           << z.last_setpoint;
      ss << "}";
    }
    ss << "],\n";

    // Telemetry
    ss << "      \"telemetry\": {";
    bool first_tel = true;
    for (const auto &tkv : dev.last_telemetry) {
      if (!first_tel)
        ss << ", ";
      first_tel = false;
      ss << "\"" << json_escape(tkv.first) << "\": " << std::fixed
         << std::setprecision(1) << tkv.second;
    }
    ss << "},\n";

    // Per-device YAML
    std::string dev_yaml = this->generate_device_yaml(dev);
    ss << "      \"yaml\": \"" << json_escape(dev_yaml) << "\"\n";
    ss << "    }";
  }

  // Include any configured devices not yet observed on RF during this session
  for (const auto &ca : configured_addrs) {
    if (this->devices_.count(ca) == 0) {
      if (!first_dev)
        ss << ",\n";
      first_dev = false;
      ss << "    {\n";
      ss << "      \"address\": \"" << json_escape(ca) << "\",\n";
      ss << "      \"device_type\": \"saved\",\n";
      ss << "      \"type_label\": \"Configured Device\",\n";
      ss << "      \"oem_name\": \"\",\n";
      ss << "      \"associated_remote\": \"\",\n";
      ss << "      \"associated_target\": \"\",\n";
      ss << "      \"rssi\": 0,\n";
      ss << "      \"last_seen_sec\": 0,\n";
      ss << "      \"has_dhw\": false,\n";
      ss << "      \"is_saved\": true,\n";
      ss << "      \"zones\": [],\n";
      ss << "      \"telemetry\": {},\n";
      ss << "      \"yaml\": \"\"\n";
      ss << "    }";
    }
  }

  ss << "\n  ],\n";

  // 2. Recent Decoded RF Packets (Live Traffic Stream)
  ss << "  \"packets\": [\n";
  bool first_pkt = true;
  for (const auto &pkt : this->recent_packets_) {
    if (!first_pkt) {
      ss << ",\n";
    }
    first_pkt = false;

    uint32_t pkt_ago_sec = 0;
    if (now_ms > 0 && pkt.timestamp_ms > 0 && now_ms >= pkt.timestamp_ms) {
      pkt_ago_sec = (now_ms - pkt.timestamp_ms) / 1000;
    }

    ss << "    {\n";
    ss << "      \"id\": " << pkt.id << ",\n";
    ss << "      \"hgi80\": \"" << json_escape(pkt.hgi80) << "\",\n";
    ss << "      \"verb\": \"" << json_escape(pkt.verb) << "\",\n";
    ss << "      \"src\": \"" << json_escape(pkt.src) << "\",\n";
    ss << "      \"dst\": \"" << json_escape(pkt.dst) << "\",\n";
    ss << "      \"opcode\": \"" << json_escape(pkt.opcode_hex) << "\",\n";
    ss << "      \"opcode_name\": \"" << json_escape(pkt.opcode_name)
       << "\",\n";
    ss << "      \"rssi\": " << (int)pkt.rssi << ",\n";
    ss << "      \"last_seen_sec\": " << pkt_ago_sec << ",\n";
    ss << "      \"fields\": [";
    bool first_f = true;
    for (const auto &f : pkt.fields) {
      if (!first_f)
        ss << ", ";
      first_f = false;
      ss << "{\"name\": \"" << json_escape(f.name) << "\", \"value\": \""
         << json_escape(f.value) << "\"}";
    }
    ss << "]\n";
    ss << "    }";
  }
  ss << "\n  ],\n";
  ss << "  \"full_yaml\": \"" << json_escape(this->generate_yaml()) << "\"\n";
  ss << "}\n";

  return ss.str();
}

std::string RamsesDiscoveryComponent::generate_yaml() const {
  std::stringstream ss;

  ss << "# ==========================================================\n";
  ss << "# Automatically Generated RAMSES Configuration\n";
  ss << "# Generated by ramses_discovery\n";
  ss << "# ==========================================================\n\n";

  // 0. Subdevices under esphome.devices
  bool has_devices = false;
  for (const auto &kv : this->devices_) {
    if (device_has_entities(kv.second)) {
      if (!has_devices) {
        ss << "esphome:\n";
        ss << "  devices:\n";
        has_devices = true;
      }
      ss << "    - id: " << sanitize_device_id(kv.second.address) << "\n";
      ss << "      name: \"" << get_device_title(kv.second) << "\"\n";
    }
  }
  if (has_devices) {
    ss << "\n";
  }

  // 1. Climate Platform
  bool has_climate = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    if (dev.device_type == "controller" && !dev.zones.empty()) {
      if (!has_climate) {
        ss << "climate:\n";
        has_climate = true;
      }
      std::string addr_str = dev.address.to_string();
      std::string dev_id = sanitize_device_id(dev.address);
      for (const auto &zkv : dev.zones) {
        const DiscoveredZone &z = zkv.second;
        std::string zone_name =
            z.name.empty() ? ("Zone " + std::to_string(z.index)) : z.name;
        ss << "  - platform: ramses_devices\n";
        ss << "    ramses_esp_id: ramses_hub\n";
        ss << "    name: \"" << zone_name << " Heating\"\n";
        ss << "    controller_address: \"" << addr_str << "\"\n";
        ss << "    zone_index: " << (int)z.index << "\n";
        ss << "    zone_name: \"" << zone_name << "\"\n";
        ss << "    device_id: " << dev_id << "\n\n";
      }
    }
  }

  // 2. Water Heater Platform
  bool has_dhw = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();
    std::string dev_id = sanitize_device_id(dev.address);

    if (dev.device_type == "controller" && dev.has_dhw) {
      if (!has_dhw) {
        ss << "water_heater:\n";
        has_dhw = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"Domestic Hot Water\"\n";
      ss << "    controller_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    }
  }

  // 3. Fan Platform
  bool has_fan = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();
    std::string dev_id = sanitize_device_id(dev.address);

    if (dev.is_hvac) {
      if (!has_fan) {
        ss << "fan:\n";
        has_fan = true;
      }
      std::string title = get_device_title(dev);
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    name: \"" << title << "\"\n";
      ss << "    device_address: \"" << addr_str << "\"\n";
      if (!dev.associated_remote.empty()) {
        ss << "    # fake_remote_address: \"" << dev.associated_remote
           << "\"  # Optional: uncomment if cloning this remote\n";
      }
      ss << "    scheme: " << dev.oem_name << "\n";
      ss << "    device_id: " << dev_id << "\n\n";
    }
  }

  // 4. Sensor Platform
  bool has_sensor = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();
    std::string dev_id = sanitize_device_id(dev.address);

    auto emit_sensor = [&](const std::string &type, const std::string &name) {
      if (!has_sensor) {
        ss << "sensor:\n";
        has_sensor = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.device_type == "trv") {
      if (dev.last_telemetry.count("heat_demand") ||
          dev.seen_opcodes.count(0x3150))
        emit_sensor("heat_demand", "TRV " + addr_str + " Heat Demand");
      if (dev.last_telemetry.count("battery_level") ||
          dev.seen_opcodes.count(0x1060))
        emit_sensor("battery_level", "TRV " + addr_str + " Battery Level");
    } else if (dev.device_type == "opentherm") {
      if (dev.last_telemetry.count("modulation"))
        emit_sensor("opentherm_modulation", "Boiler Modulation");
      if (dev.last_telemetry.count("flow_temperature"))
        emit_sensor("flow_temperature", "Boiler Flow Temperature");
      if (dev.last_telemetry.count("return_temperature"))
        emit_sensor("return_temperature", "Boiler Return Temperature");
    } else if (dev.is_hvac) {
      if (dev.last_telemetry.count("supply_temperature"))
        emit_sensor("supply_temperature", "Ventilation Supply Temperature");
      if (dev.last_telemetry.count("exhaust_temperature"))
        emit_sensor("exhaust_temperature", "Ventilation Exhaust Temperature");
      if (dev.last_telemetry.count("indoor_temperature") ||
          dev.last_telemetry.count("air_quality_temperature"))
        emit_sensor("air_quality_temperature",
                    "Ventilation Indoor Temperature");
      if (dev.last_telemetry.count("outdoor_temperature"))
        emit_sensor("outdoor_temperature", "Ventilation Outdoor Temperature");
      if (dev.last_telemetry.count("bypass_position"))
        emit_sensor("bypass_position", "Ventilation Bypass Position");
      if (dev.last_telemetry.count("supply_fan_speed"))
        emit_sensor("supply_fan_speed", "Ventilation Supply Fan Speed");
      if (dev.last_telemetry.count("exhaust_fan_speed"))
        emit_sensor("exhaust_fan_speed", "Ventilation Exhaust Fan Speed");
      if (dev.last_telemetry.count("timer_minutes") ||
          dev.last_telemetry.count("remaining_mins"))
        emit_sensor("remaining_mins", "Ventilation Remaining Timer");
      if (dev.last_telemetry.count("filter_remaining_days"))
        emit_sensor("filter_remaining_days",
                    "Ventilation Filter Remaining Days");
      if (dev.last_telemetry.count("filter_lifetime_days"))
        emit_sensor("filter_lifetime_days", "Ventilation Filter Lifetime");
      if (dev.last_telemetry.count("filter_remaining_percent"))
        emit_sensor("filter_remaining_percent",
                    "Ventilation Filter Remaining Percent");
      if (dev.last_telemetry.count("indoor_humidity"))
        emit_sensor("indoor_humidity", "Ventilation Indoor Humidity");
      if (dev.last_telemetry.count("outdoor_humidity"))
        emit_sensor("outdoor_humidity", "Ventilation Outdoor Humidity");
      if (dev.last_telemetry.count("co2"))
        emit_sensor("co2", "Ventilation CO2 Level");
    } else if (dev.device_type == "relay") {
      if (dev.last_telemetry.count("relay_demand") ||
          dev.seen_opcodes.count(0x0008)) {
        if (!has_sensor) {
          ss << "sensor:\n";
          has_sensor = true;
        }
        ss << "  - platform: ramses_devices\n";
        ss << "    ramses_esp_id: ramses_hub\n";
        ss << "    type: relay_demand\n";
        ss << "    name: \"Relay " << addr_str << " Demand\"\n";
        ss << "    ramses_address: \"" << addr_str << "\"\n";
        ss << "    relay_index: 0\n";
        ss << "    device_id: " << dev_id << "\n\n";
      }
    } else if (dev.device_type == "sensor") {
      if (dev.last_telemetry.count("outdoor_temperature"))
        emit_sensor("outdoor_temperature",
                    "Sensor " + addr_str + " Outdoor Temperature");
      if (dev.last_telemetry.count("indoor_temperature"))
        emit_sensor("air_quality_temperature",
                    "Sensor " + addr_str + " Indoor Temperature");
      if (dev.last_telemetry.count("indoor_humidity"))
        emit_sensor("indoor_humidity",
                    "Sensor " + addr_str + " Indoor Humidity");
      if (dev.last_telemetry.count("co2"))
        emit_sensor("co2", "Sensor " + addr_str + " CO2 Level");
    }
  }

  // 5. Binary Sensor Platform
  bool has_bin = false;
  for (const auto &kv : this->devices_) {
    const DiscoveredDevice &dev = kv.second;
    std::string addr_str = dev.address.to_string();
    std::string dev_id = sanitize_device_id(dev.address);

    auto emit_bin = [&](const std::string &type, const std::string &name) {
      if (!has_bin) {
        ss << "binary_sensor:\n";
        has_bin = true;
      }
      ss << "  - platform: ramses_devices\n";
      ss << "    ramses_esp_id: ramses_hub\n";
      ss << "    type: " << type << "\n";
      ss << "    name: \"" << name << "\"\n";
      ss << "    ramses_address: \"" << addr_str << "\"\n";
      ss << "    device_id: " << dev_id << "\n\n";
    };

    if (dev.device_type == "trv") {
      if (dev.last_telemetry.count("battery_low") ||
          dev.last_telemetry.count("battery_level") ||
          dev.seen_opcodes.count(0x1060))
        emit_bin("battery_low", "TRV " + addr_str + " Battery Low Warning");
    } else if (dev.device_type == "opentherm") {
      if (dev.last_telemetry.count("flame_active"))
        emit_bin("flame_active", "Boiler Flame Active");
      if (dev.last_telemetry.count("fault") ||
          dev.last_telemetry.count("fault_code"))
        emit_bin("fault_alarm", "Boiler Fault Warning");
    } else if (dev.is_hvac) {
      if (dev.last_telemetry.count("filter_dirty"))
        emit_bin("filter_alarm", "Ventilation Filter Dirty Warning");
      if (dev.last_telemetry.count("bypass_active"))
        emit_bin("bypass_active", "Ventilation Bypass Active");
    }
  }

  return ss.str();
}

void RamsesDiscoveryComponent::dump_yaml() const {
  std::string yaml = this->generate_yaml();
  std::istringstream stream(yaml);
  std::string line;
  while (std::getline(stream, line)) {
    ESP_LOGI(TAG, "%s", line.c_str());
  }
}

} // namespace ramses_discovery
} // namespace esphome
