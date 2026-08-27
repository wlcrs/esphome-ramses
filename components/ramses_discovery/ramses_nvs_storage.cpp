#if __has_include("esphome/components/ramses_discovery/ramses_nvs_storage.h")
#include "esphome/components/ramses_discovery/ramses_nvs_storage.h"
#else
#include "components/ramses_discovery/ramses_nvs_storage.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cstring>
#include <sstream>

#ifdef USE_ESP_IDF
#include <nvs.h>
#include <nvs_flash.h>
#endif

#ifdef USE_JSON
#include "esphome/components/json/json_util.h"
#endif

namespace esphome {
namespace ramses_discovery {

static const char *const TAG = "ramses_nvs";
static const char *const NVS_NAMESPACE = "ramses_cfg";
static const char *const NVS_CONFIG_KEY = "config_blob";

#ifndef USE_ESP_IDF
// Host mock buffer for native unit tests
static std::string g_mock_nvs_storage;
#endif

bool RamsesNvsStorage::save_config(const std::string &json_str) {
  this->configured_addresses_.clear();
#ifdef USE_ESP_IDF
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s", NVS_NAMESPACE,
             esp_err_to_name(err));
    return false;
  }

  // ESP-IDF 5 Multi-Page Blobs support arbitrary size blobs
  err = nvs_set_blob(handle, NVS_CONFIG_KEY, json_str.data(), json_str.size());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write NVS config blob: %s", esp_err_to_name(err));
    nvs_close(handle);
    return false;
  }

  err = nvs_commit(handle);
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS config: %s", esp_err_to_name(err));
    return false;
  }
  ESP_LOGI(TAG, "Saved %u bytes of device configuration to NVS",
           (unsigned)json_str.size());
  return true;
#else
  g_mock_nvs_storage = json_str;
  return true;
#endif
}

std::string RamsesNvsStorage::load_config() {
#ifdef USE_ESP_IDF
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    return "";
  }

  size_t required_size = 0;
  err = nvs_get_blob(handle, NVS_CONFIG_KEY, nullptr, &required_size);
  if (err != ESP_OK || required_size == 0) {
    nvs_close(handle);
    return "";
  }

  std::string result;
  result.resize(required_size);
  err = nvs_get_blob(handle, NVS_CONFIG_KEY, &result[0], &required_size);
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read NVS config blob: %s", esp_err_to_name(err));
    return "";
  }
  return result;
#else
  return g_mock_nvs_storage;
#endif
}

bool RamsesNvsStorage::clear_config() {
  this->configured_addresses_.clear();
#ifdef USE_ESP_IDF
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return false;
  }
  nvs_erase_key(handle, NVS_CONFIG_KEY);
  nvs_commit(handle);
  nvs_close(handle);
  ESP_LOGI(TAG, "Erased device configuration from NVS");
  return true;
#else
  g_mock_nvs_storage.clear();
  return true;
#endif
}

bool RamsesNvsStorage::has_config() {
  std::string cfg = this->load_config();
  return !cfg.empty() && cfg.find("\"devices\"") != std::string::npos;
}

// Minimal portable JSON helper for native tests & firmware
static bool parse_json_object(const std::string &json, size_t start,
                              size_t &end_pos, std::string &block) {
  size_t open_brace = json.find('{', start);
  if (open_brace == std::string::npos)
    return false;
  int depth = 0;
  bool in_quotes = false;
  bool escape = false;
  for (size_t i = open_brace; i < json.size(); ++i) {
    char c = json[i];
    if (escape) {
      escape = false;
      continue;
    }
    if (c == '\\') {
      escape = true;
      continue;
    }
    if (c == '"') {
      in_quotes = !in_quotes;
      continue;
    }
    if (!in_quotes) {
      if (c == '{')
        depth++;
      else if (c == '}') {
        depth--;
        if (depth == 0) {
          end_pos = i + 1;
          block = json.substr(open_brace, end_pos - open_brace);
          return true;
        }
      }
    }
  }
  return false;
}

static std::string json_get_str(const std::string &obj,
                                const std::string &key) {
  size_t k = obj.find("\"" + key + "\"");
  if (k == std::string::npos)
    return "";
  size_t colon = obj.find(':', k);
  if (colon == std::string::npos)
    return "";
  size_t quote_start = obj.find('"', colon + 1);
  if (quote_start == std::string::npos)
    return "";
  size_t quote_end = obj.find('"', quote_start + 1);
  if (quote_end == std::string::npos)
    return "";
  return obj.substr(quote_start + 1, quote_end - quote_start - 1);
}

static bool json_get_bool(const std::string &obj, const std::string &key,
                          bool default_val = false) {
  size_t k = obj.find("\"" + key + "\"");
  if (k == std::string::npos)
    return default_val;
  size_t colon = obj.find(':', k);
  if (colon == std::string::npos)
    return default_val;
  size_t val_start = obj.find_first_not_of(" \t\r\n", colon + 1);
  if (val_start == std::string::npos)
    return default_val;
  if (obj.compare(val_start, 4, "true") == 0 ||
      obj.compare(val_start, 1, "1") == 0)
    return true;
  if (obj.compare(val_start, 5, "false") == 0 ||
      obj.compare(val_start, 1, "0") == 0)
    return false;
  return default_val;
}

static int json_get_int(const std::string &obj, const std::string &key,
                        int default_val = 0) {
  size_t k = obj.find("\"" + key + "\"");
  if (k == std::string::npos)
    return default_val;
  size_t colon = obj.find(':', k);
  if (colon == std::string::npos)
    return default_val;
  size_t val_start = obj.find_first_not_of(" \t\r\n", colon + 1);
  if (val_start == std::string::npos)
    return default_val;
  return std::strtol(obj.c_str() + val_start, nullptr, 10);
}

bool RamsesNvsStorage::is_device_configured(const std::string &address) {
  if (this->configured_addresses_.empty() && this->has_config()) {
    std::string cfg = this->load_config();
    size_t devs_start = cfg.find("\"devices\"");
    if (devs_start != std::string::npos) {
      size_t arr_start = cfg.find('[', devs_start);
      if (arr_start != std::string::npos) {
        size_t pos = arr_start + 1;
        while (pos < cfg.size()) {
          size_t end_pos = 0;
          std::string dev_block;
          if (!parse_json_object(cfg, pos, end_pos, dev_block))
            break;
          pos = end_pos;
          std::string addr = json_get_str(dev_block, "address");
          if (addr.empty())
            addr = json_get_str(dev_block, "addr");
          if (!addr.empty())
            this->configured_addresses_.insert(addr);
        }
      }
    }
  }
  return this->configured_addresses_.count(address) > 0;
}

const std::set<std::string> &RamsesNvsStorage::get_configured_addresses() {
  if (this->configured_addresses_.empty()) {
    this->is_device_configured("");
  }
  return this->configured_addresses_;
}

size_t RamsesNvsStorage::load_and_register_entities(
    ramses_esp::RamsesESPComponent *hub) {
  std::string cfg = this->load_config();
  if (cfg.empty()) {
    return 0;
  }
  return this->instantiate_from_json(cfg, hub);
}

size_t
RamsesNvsStorage::instantiate_from_json(const std::string &json_str,
                                        ramses_esp::RamsesESPComponent *hub) {
  if (json_str.empty())
    return 0;

  size_t devs_start = json_str.find("\"devices\"");
  if (devs_start == std::string::npos)
    return 0;

  size_t arr_start = json_str.find('[', devs_start);
  if (arr_start == std::string::npos)
    return 0;

  size_t count = 0;
  size_t pos = arr_start + 1;

  while (pos < json_str.size()) {
    size_t end_pos = 0;
    std::string dev_block;
    if (!parse_json_object(json_str, pos, end_pos, dev_block)) {
      break;
    }
    pos = end_pos;

    std::string addr = json_get_str(dev_block, "address");
    if (addr.empty()) {
      addr = json_get_str(dev_block, "addr");
    }
    if (addr.empty())
      continue;

    this->configured_addresses_.insert(addr);

    std::string type = json_get_str(dev_block, "type");
    std::string dev_name = json_get_str(dev_block, "name");
    if (dev_name.empty()) {
      dev_name = "RAMSES " + addr;
    }

    // 1. Controller Devices
    if (type == "controller" || type == "ctl" || type == "evohome") {
      // Parse zones array
      size_t zones_idx = dev_block.find("\"zones\"");
      if (zones_idx != std::string::npos) {
        size_t z_arr_start = dev_block.find('[', zones_idx);
        size_t z_pos = (z_arr_start != std::string::npos) ? z_arr_start + 1
                                                          : std::string::npos;
        while (z_pos < dev_block.size()) {
          size_t z_end = 0;
          std::string z_block;
          if (!parse_json_object(dev_block, z_pos, z_end, z_block)) {
            break;
          }
          z_pos = z_end;

          uint8_t z_idx = static_cast<uint8_t>(
              json_get_int(z_block, "index", json_get_int(z_block, "idx", 0)));
          std::string z_name = json_get_str(z_block, "name");
          if (z_name.empty()) {
            z_name = "Zone " + std::to_string(z_idx);
          }

          bool has_climate = json_get_bool(z_block, "climate", true);
          bool has_temp = json_get_bool(z_block, "temp", true);
          bool has_setpoint = json_get_bool(z_block, "setpoint", true);

#ifdef USE_CLIMATE
          if (has_climate) {
            auto *climate = new ramses_devices::RamsesClimate();
            climate->set_parent(hub);
            climate->set_controller_address(addr);
            climate->set_zone_index(z_idx);
            climate->set_zone_name(z_name);
            climate->setup();
            std::string c_name = z_name + " Heating";
            App.register_climate(climate, this->intern_string(c_name), 0, 0);
            count++;
          }
#endif

#ifdef USE_SENSOR
          if (has_temp) {
            auto *s_temp = new ramses_devices::RamsesSensor();
            s_temp->set_parent(hub);
            s_temp->set_device_address(addr);
            s_temp->set_zone_index(z_idx);
            s_temp->set_sensor_type(
                ramses_devices::RamsesSensorType::ZONE_TEMPERATURE);
            s_temp->set_accuracy_decimals(1);
            s_temp->setup();
            std::string st_name = z_name + " Temperature";
            App.register_sensor(s_temp, this->intern_string(st_name), 0, 0);
            count++;
          }
          if (has_setpoint) {
            auto *s_sp = new ramses_devices::RamsesSensor();
            s_sp->set_parent(hub);
            s_sp->set_device_address(addr);
            s_sp->set_zone_index(z_idx);
            s_sp->set_sensor_type(
                ramses_devices::RamsesSensorType::ZONE_SETPOINT);
            s_sp->set_accuracy_decimals(1);
            s_sp->setup();
            std::string sp_name = z_name + " Setpoint";
            App.register_sensor(s_sp, this->intern_string(sp_name), 0, 0);
            count++;
          }
#endif
        }
      }

      bool has_dhw = json_get_bool(dev_block, "dhw", false);
#ifdef USE_WATER_HEATER
      if (has_dhw) {
        auto *wh = new ramses_devices::RamsesWaterHeater();
        wh->set_parent(hub);
        wh->set_controller_address(addr);
        wh->setup();
        std::string wh_name = dev_name + " Hot Water";
        App.register_water_heater(wh, this->intern_string(wh_name), 0, 0);
        count++;
      }
#endif
    }

    // 2. HVAC / Ventilation Units
    else if (type == "hvac" || type == "fan" || type == "ventilation") {
      bool enable_fan = json_get_bool(dev_block, "fan", true);
      std::string scheme_str = json_get_str(dev_block, "scheme");
      ramses_esp::HvacScheme scheme = ramses_esp::HvacScheme::ORCON;
      if (scheme_str == "vasco")
        scheme = ramses_esp::HvacScheme::VASCO;
      else if (scheme_str == "itho")
        scheme = ramses_esp::HvacScheme::ITHO;
      else if (scheme_str == "zehnder")
        scheme = ramses_esp::HvacScheme::ZEHNDER;
      else if (scheme_str == "hopper")
        scheme = ramses_esp::HvacScheme::HOPPER;

#ifdef USE_FAN
      if (enable_fan) {
        auto *fan = new ramses_devices::RamsesFan();
        fan->set_parent(hub);
        fan->set_device_address(addr);
        fan->set_scheme(scheme);
        std::string fake_rem = json_get_str(dev_block, "fake_remote_address");
        if (!fake_rem.empty()) {
          fan->set_fake_remote_address(fake_rem);
        }
        fan->setup();
        this->add_dynamic_component(fan);
        App.register_fan(fan, this->intern_string(dev_name), 0, 0);
        count++;

#ifdef USE_BUTTON
        auto *pbtn = new ramses_devices::RamsesFanPairingButton();
        pbtn->set_fan(fan);
        pbtn->setup();
        std::string oem_label = scheme_str.empty() ? "Fan" : scheme_str;
        oem_label[0] = std::toupper(oem_label[0]);
        std::string btn_name = "Pair with " + oem_label;
        App.register_button(pbtn, this->intern_string(btn_name), 0, 0);
        count++;
#endif
      }
#endif

#ifndef RAMSES_FIELDS_TEMP
#define RAMSES_FIELDS_TEMP 0
#endif
#ifndef RAMSES_FIELDS_HUMIDITY
#define RAMSES_FIELDS_HUMIDITY 0
#endif
#ifndef RAMSES_FIELDS_CO2
#define RAMSES_FIELDS_CO2 0
#endif
#ifndef RAMSES_FIELDS_PERCENT
#define RAMSES_FIELDS_PERCENT 0
#endif
#ifndef RAMSES_FIELDS_BATTERY
#define RAMSES_FIELDS_BATTERY 0
#endif
#ifndef RAMSES_FIELDS_MINUTES
#define RAMSES_FIELDS_MINUTES 0
#endif
#ifndef RAMSES_FIELDS_DAYS
#define RAMSES_FIELDS_DAYS 0
#endif
#ifndef RAMSES_FIELDS_BIN_PROBLEM
#define RAMSES_FIELDS_BIN_PROBLEM 0
#endif
#ifndef RAMSES_FIELDS_BIN_WINDOW
#define RAMSES_FIELDS_BIN_WINDOW 0
#endif
#ifndef RAMSES_FIELDS_BIN_RUNNING
#define RAMSES_FIELDS_BIN_RUNNING 0
#endif
#ifndef RAMSES_FIELDS_BIN_BATTERY
#define RAMSES_FIELDS_BIN_BATTERY 0
#endif

#ifdef USE_SENSOR
      if (json_get_bool(dev_block, "supply_temperature", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::SUPPLY_TEMPERATURE);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Supply Temperature";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
      if (json_get_bool(dev_block, "exhaust_temperature", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::EXHAUST_TEMPERATURE);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Exhaust Temperature";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
      if (json_get_bool(
              dev_block, "indoor_temperature",
              json_get_bool(dev_block, "air_quality_temperature", false))) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::AIR_QUALITY_TEMPERATURE);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Indoor Temperature";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
      if (json_get_bool(dev_block, "outdoor_temperature", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::OUTDOOR_TEMPERATURE);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Outdoor Temperature";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
      if (json_get_bool(dev_block, "bypass_position", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::BYPASS_POSITION);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Bypass Position";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
      if (json_get_bool(dev_block, "supply_fan_speed", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::SUPPLY_FAN_SPEED);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Supply Fan Speed";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
      if (json_get_bool(dev_block, "exhaust_fan_speed", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::EXHAUST_FAN_SPEED);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Exhaust Fan Speed";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
      if (json_get_bool(dev_block, "remaining_mins",
                        json_get_bool(dev_block, "timer_minutes", false))) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::REMAINING_MINS);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Remaining Timer";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_MINUTES);
        count++;
      }
      if (json_get_bool(dev_block, "co2", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::CO2);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " CO2";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_CO2);
        count++;
      }
      if (json_get_bool(dev_block, "indoor_humidity",
                        json_get_bool(dev_block, "humidity", false))) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::INDOOR_HUMIDITY);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Humidity";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_HUMIDITY);
        count++;
      }
      if (json_get_bool(dev_block, "outdoor_humidity", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::OUTDOOR_HUMIDITY);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Outdoor Humidity";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_HUMIDITY);
        count++;
      }
      if (json_get_bool(dev_block, "filter_remaining_days", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::FILTER_REMAINING_DAYS);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Filter Remaining Days";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_DAYS);
        count++;
      }
      if (json_get_bool(dev_block, "filter_lifetime_days", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::FILTER_LIFETIME_DAYS);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Filter Lifetime";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_DAYS);
        count++;
      }
      if (json_get_bool(dev_block, "filter_remaining_percent", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::FILTER_REMAINING_PERCENT);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Filter Remaining Percent";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
#endif

#ifdef USE_BINARY_SENSOR
      if (json_get_bool(dev_block, "filter_alarm",
                        json_get_bool(dev_block, "filter_dirty", false))) {
        auto *bs = new ramses_devices::RamsesBinarySensor();
        bs->set_parent(hub);
        bs->set_device_address(addr);
        bs->set_sensor_type(
            ramses_devices::RamsesBinarySensorType::FILTER_ALARM);
        bs->setup();
        std::string n = dev_name + " Filter Warning";
        App.register_binary_sensor(bs, this->intern_string(n), 0,
                                   RAMSES_FIELDS_BIN_PROBLEM);
        count++;
      }
      if (json_get_bool(dev_block, "bypass_active", false)) {
        auto *bs = new ramses_devices::RamsesBinarySensor();
        bs->set_parent(hub);
        bs->set_device_address(addr);
        bs->set_sensor_type(
            ramses_devices::RamsesBinarySensorType::BYPASS_ACTIVE);
        bs->setup();
        std::string n = dev_name + " Bypass Active";
        App.register_binary_sensor(bs, this->intern_string(n), 0, 0);
        count++;
      }
#endif
    }

    // 3. TRV Radiator Valves
    else if (type == "trv") {
#ifdef USE_SENSOR
      if (json_get_bool(dev_block, "heat_demand", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::HEAT_DEMAND);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Heat Demand";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
      if (json_get_bool(dev_block, "battery_level", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::BATTERY_LEVEL);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Battery";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_BATTERY);
        count++;
      }
#endif
#ifdef USE_BINARY_SENSOR
      if (json_get_bool(dev_block, "battery_low", true)) {
        auto *bs = new ramses_devices::RamsesBinarySensor();
        bs->set_parent(hub);
        bs->set_device_address(addr);
        bs->set_sensor_type(
            ramses_devices::RamsesBinarySensorType::BATTERY_LOW);
        bs->setup();
        std::string n = dev_name + " Battery Low";
        App.register_binary_sensor(bs, this->intern_string(n), 0,
                                   RAMSES_FIELDS_BIN_BATTERY);
        count++;
      }
#endif
    }

    // 4. OpenTherm Bridge
    else if (type == "opentherm" || type == "otb") {
#ifdef USE_SENSOR
      if (json_get_bool(dev_block, "opentherm_modulation", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::OPENTHERM_MODULATION);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Modulation";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
      if (json_get_bool(dev_block, "flow_temperature", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::OPENTHERM_FLOW_TEMP);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Flow Temp";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
      if (json_get_bool(dev_block, "return_temperature", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::OPENTHERM_RETURN_TEMP);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Return Temp";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
#endif
#ifdef USE_BINARY_SENSOR
      if (json_get_bool(dev_block, "flame_active", true)) {
        auto *bs = new ramses_devices::RamsesBinarySensor();
        bs->set_parent(hub);
        bs->set_device_address(addr);
        bs->set_sensor_type(
            ramses_devices::RamsesBinarySensorType::FLAME_ACTIVE);
        bs->setup();
        std::string n = dev_name + " Flame";
        App.register_binary_sensor(bs, this->intern_string(n), 0,
                                   RAMSES_FIELDS_BIN_RUNNING);
        count++;
      }
      if (json_get_bool(dev_block, "fault_alarm", true)) {
        auto *bs = new ramses_devices::RamsesBinarySensor();
        bs->set_parent(hub);
        bs->set_device_address(addr);
        bs->set_sensor_type(
            ramses_devices::RamsesBinarySensorType::FAULT_ALARM);
        bs->setup();
        std::string n = dev_name + " Fault Warning";
        App.register_binary_sensor(bs, this->intern_string(n), 0,
                                   RAMSES_FIELDS_BIN_PROBLEM);
        count++;
      }
#endif
    }

    // 5. Relay Actuator
    else if (type == "relay" || type == "bdr91") {
#ifdef USE_SENSOR
      if (json_get_bool(dev_block, "relay_demand", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::RELAY_DEMAND);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Demand";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_PERCENT);
        count++;
      }
#endif
    }

    // 6. Generic Sensors (Room thermostat / Remote sensor / CO2 / Humidity)
    else if (type == "sensor") {
#ifdef USE_SENSOR
      if (json_get_bool(dev_block, "temperature", true)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(
            ramses_devices::RamsesSensorType::AIR_QUALITY_TEMPERATURE);
        s->set_accuracy_decimals(1);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Temperature";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_TEMP);
        count++;
      }
      if (json_get_bool(dev_block, "indoor_humidity", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::INDOOR_HUMIDITY);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " Humidity";
        App.register_sensor(s, this->intern_string(n), 0,
                            RAMSES_FIELDS_HUMIDITY);
        count++;
      }
      if (json_get_bool(dev_block, "co2", false)) {
        auto *s = new ramses_devices::RamsesSensor();
        s->set_parent(hub);
        s->set_device_address(addr);
        s->set_sensor_type(ramses_devices::RamsesSensorType::CO2);
        s->set_accuracy_decimals(0);
        s->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
        s->setup();
        std::string n = dev_name + " CO2";
        App.register_sensor(s, this->intern_string(n), 0, RAMSES_FIELDS_CO2);
        count++;
      }
#endif
    }
  }

  this->is_configured_ = (count > 0);
  this->entity_count_ = count;
  ESP_LOGI(
      TAG,
      "Dynamically instantiated and registered %u entity/entities from NVS",
      (unsigned)count);
  return count;
}

} // namespace ramses_discovery
} // namespace esphome
