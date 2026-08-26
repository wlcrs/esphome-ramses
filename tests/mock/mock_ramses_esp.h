#pragma once

#include "components/ramses_esp/ramses_message.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace ramses_esp {

class MockRamsesEsp;

// Base class for simulated RAMSES II devices
class MockRamsesDevice {
public:
  explicit MockRamsesDevice(std::string device_id)
      : device_id_(std::move(device_id)) {}
  virtual ~MockRamsesDevice() = default;

  const std::string &get_device_id() const { return this->device_id_; }

  virtual void handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) {}
  virtual void tick(uint32_t now_ms, MockRamsesEsp &hub) {}

protected:
  std::string device_id_;
};

// ----------------------------------------------------------------------
// Mock Evohome Multi-Zone Heating Controller (01:xxxxxx)
// ----------------------------------------------------------------------
struct MockZoneConfig {
  uint8_t index{0};
  std::string name{"Zone"};
  float current_temp{20.0f};
  float target_setpoint{20.0f};
  float heat_demand{0.0f}; // 0.0 - 1.0 (0% - 100%)
};

class MockEvohomeController : public MockRamsesDevice {
public:
  explicit MockEvohomeController(std::string controller_id);

  void add_zone(uint8_t index, const std::string &name,
                float current_temp = 20.0f, float setpoint = 20.0f);
  void set_zone_temp(uint8_t index, float temp);
  void set_zone_setpoint(uint8_t index, float setpoint);
  void set_system_mode(uint8_t mode);

  uint8_t get_system_mode() const { return this->system_mode_; }
  const std::vector<MockZoneConfig> &get_zones() const { return this->zones_; }

  void handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) override;
  void broadcast_sync(MockRamsesEsp &hub);
  void broadcast_temperatures(MockRamsesEsp &hub);
  void broadcast_setpoints(MockRamsesEsp &hub);

private:
  uint8_t system_mode_{0}; // 0=Auto, 1=Away, 2=DayOff, 3=Custom, 4=Eco, 5=Off
  std::vector<MockZoneConfig> zones_;
};

// ----------------------------------------------------------------------
// Mock MVHR / Ventilation Unit (32:xxxxxx)
// ----------------------------------------------------------------------
enum MockHvacScheme {
  MOCK_SCHEME_ORCON = 0,
  MOCK_SCHEME_ITHO = 1,
  MOCK_SCHEME_VASCO = 2,
  MOCK_SCHEME_ZEHNDER = 3,
};

class MockMvhrVentilator : public MockRamsesDevice {
public:
  MockMvhrVentilator(std::string device_id,
                     MockHvacScheme scheme = MOCK_SCHEME_ORCON);

  void set_fan_mode(uint8_t mode) { this->fan_mode_ = mode; }
  uint8_t get_fan_mode() const { return this->fan_mode_; }
  MockHvacScheme get_scheme() const { return this->scheme_; }

  void set_indoor_telemetry(float temp, float humidity) {
    this->indoor_temp_ = temp;
    this->indoor_humidity_ = humidity;
  }

  void set_outdoor_telemetry(float temp, float humidity) {
    this->outdoor_temp_ = temp;
    this->outdoor_humidity_ = humidity;
  }

  void handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) override;
  void broadcast_status(MockRamsesEsp &hub);

private:
  MockHvacScheme scheme_{MOCK_SCHEME_ORCON};
  uint8_t oem_code_{0x67}; // 0x67=Orcon, 0x08=Itho, 0x13=Vasco, 0x02=Zehnder
  uint8_t fan_mode_{0x02}; // e.g. Speed 1 / Low
  float indoor_temp_{21.5f};
  float indoor_humidity_{52.0f};
  float outdoor_temp_{14.2f};
  float outdoor_humidity_{78.0f};
  uint16_t filter_days_remaining_{180};
};

// ----------------------------------------------------------------------
// Mock TRV Radiator Valve (04:xxxxxx)
// ----------------------------------------------------------------------
class MockTrv : public MockRamsesDevice {
public:
  explicit MockTrv(std::string device_id);

  void set_battery_percent(uint8_t pct) { this->battery_pct_ = pct; }
  void set_demand(float demand) { this->demand_ = demand; }

  void handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) override;
  void broadcast_telemetry(MockRamsesEsp &hub);

private:
  uint8_t battery_pct_{95};
  float demand_{0.20f};
};

// ----------------------------------------------------------------------
// Mock OpenTherm Bridge (10:xxxxxx)
// ----------------------------------------------------------------------
class MockOpenThermBridge : public MockRamsesDevice {
public:
  explicit MockOpenThermBridge(std::string device_id);

  void set_modulation(float mod_pct) { this->modulation_pct_ = mod_pct; }
  void set_temperatures(float flow_temp, float return_temp) {
    this->flow_temp_ = flow_temp;
    this->return_temp_ = return_temp;
  }
  void set_flame(bool active) { this->flame_active_ = active; }

  void handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) override;
  void broadcast_telemetry(MockRamsesEsp &hub);

private:
  float modulation_pct_{35.0f};
  float flow_temp_{55.0f};
  float return_temp_{42.5f};
  bool flame_active_{true};
  bool fault_active_{false};
};

// ----------------------------------------------------------------------
// Universal Mock Ramses Hub
// ----------------------------------------------------------------------
using PacketCallback = std::function<void(const RamsesMessage &msg,
                                          const std::string &hgi80_line)>;

class MockRamsesEsp {
public:
  MockRamsesEsp();

  void register_device(std::shared_ptr<MockRamsesDevice> dev);
  std::shared_ptr<MockRamsesDevice>
  get_device(const std::string &device_id) const;

  void set_packet_callback(PacketCallback cb) {
    this->callback_ = std::move(cb);
  }

  // Inject a message as if received from RF transceiver
  void inject_message(const RamsesMessage &msg);
  void inject_hgi80(const std::string &hgi80_line);

  // Transmit a message from a simulated device to the network
  void transmit_message(const RamsesMessage &msg);

  // Tick all simulated devices
  void tick(uint32_t now_ms);

  const std::vector<std::string> &get_tx_log() const { return this->tx_log_; }
  void clear_tx_log() { this->tx_log_.clear(); }

private:
  std::map<std::string, std::shared_ptr<MockRamsesDevice>> devices_;
  PacketCallback callback_;
  std::vector<std::string> tx_log_;
};

} // namespace ramses_esp
} // namespace esphome
