#include "mock_ramses_esp.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace ramses_esp {

// ----------------------------------------------------------------------
// MockEvohomeController Implementation
// ----------------------------------------------------------------------
MockEvohomeController::MockEvohomeController(std::string controller_id)
    : MockRamsesDevice(std::move(controller_id)) {}

void MockEvohomeController::add_zone(uint8_t index, const std::string &name,
                                     float current_temp, float setpoint) {
  MockZoneConfig z;
  z.index = index;
  z.name = name;
  z.current_temp = current_temp;
  z.target_setpoint = setpoint;
  this->zones_.push_back(z);
}

void MockEvohomeController::set_zone_temp(uint8_t index, float temp) {
  for (auto &z : this->zones_) {
    if (z.index == index) {
      z.current_temp = temp;
      break;
    }
  }
}

void MockEvohomeController::set_zone_setpoint(uint8_t index, float setpoint) {
  for (auto &z : this->zones_) {
    if (z.index == index) {
      z.target_setpoint = setpoint;
      break;
    }
  }
}

void MockEvohomeController::set_system_mode(uint8_t mode) {
  this->system_mode_ = mode;
}

void MockEvohomeController::handle_incoming(const RamsesMessage &msg,
                                            MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  RamsesAddress src_addr = RamsesAddress::from_bytes(msg.addr[0]);
  RamsesAddress dst_addr = RamsesAddress::from_bytes(msg.addr[1]);

  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  // Handle Query (RQ) to Controller
  if (msg.type == RAMSES_MSG_RQ && dst_addr.to_string() == this->device_id_) {
    // RQ 0004: Zone Name Query
    if (opcode == 0x0004 && msg.len >= 1) {
      uint8_t requested_zone = msg.payload[0];
      for (const auto &z : this->zones_) {
        if (z.index == requested_zone) {
          uint8_t payload[22]{0};
          payload[0] = z.index;
          payload[1] = 0x00;
          size_t name_len = std::min(z.name.length(), (size_t)20);
          std::memcpy(&payload[2], z.name.data(), name_len);

          RamsesMessage rp = RamsesMessageBuilder::reply()
                                 .from(my_addr)
                                 .to(src_addr)
                                 .opcode(0x0004)
                                 .payload(payload, 22);
          hub.transmit_message(rp);
          break;
        }
      }
    }
    // RQ 0005: Zone Structure Query
    else if (opcode == 0x0005) {
      uint16_t mask = 0;
      for (const auto &z : this->zones_) {
        if (z.index < 16)
          mask |= (1 << z.index);
      }
      uint8_t payload[4] = {0x00, static_cast<uint8_t>((mask >> 8) & 0xFF),
                            static_cast<uint8_t>(mask & 0xFF),
                            static_cast<uint8_t>(this->zones_.size())};
      RamsesMessage rp = RamsesMessageBuilder::reply()
                             .from(my_addr)
                             .to(src_addr)
                             .opcode(0x0005)
                             .payload(payload, 4);
      hub.transmit_message(rp);
    }
    // RQ 1F09: System Sync / Mode Query
    else if (opcode == 0x1F09) {
      uint8_t payload[3] = {this->system_mode_, 0x07, 0xD0};
      RamsesMessage rp = RamsesMessageBuilder::reply()
                             .from(my_addr)
                             .to(src_addr)
                             .opcode(0x1F09)
                             .payload(payload, 3);
      hub.transmit_message(rp);
    }
    // RQ 2309: Setpoint Query
    else if (opcode == 0x2309 && msg.len >= 1) {
      uint8_t requested_zone = msg.payload[0];
      for (const auto &z : this->zones_) {
        if (z.index == requested_zone) {
          uint16_t sp_raw = (uint16_t)(z.target_setpoint * 100.0f);
          uint8_t payload[3] = {z.index,
                                static_cast<uint8_t>((sp_raw >> 8) & 0xFF),
                                static_cast<uint8_t>(sp_raw & 0xFF)};
          RamsesMessage rp = RamsesMessageBuilder::reply()
                                 .from(my_addr)
                                 .to(src_addr)
                                 .opcode(0x2309)
                                 .payload(payload, 3);
          hub.transmit_message(rp);
          break;
        }
      }
    }
  }
  // Handle Write (W) from Gateway (e.g. Set Setpoint or Set Mode)
  else if (msg.type == RAMSES_MSG_W &&
           (dst_addr.to_string() == this->device_id_ || !dst_addr.is_valid)) {
    // W 2309: Set Zone Setpoint
    if (opcode == 0x2309 && msg.len >= 3) {
      uint8_t z_idx = msg.payload[0];
      uint16_t sp_raw = ((uint16_t)msg.payload[1] << 8) | msg.payload[2];
      float new_sp = sp_raw / 100.0f;
      this->set_zone_setpoint(z_idx, new_sp);

      // Echo update as broadcast I 2309
      uint8_t payload[3] = {z_idx, msg.payload[1], msg.payload[2]};
      RamsesMessage i_msg = RamsesMessageBuilder::info()
                                .from(my_addr)
                                .addr2(my_addr)
                                .opcode(0x2309)
                                .payload(payload, 3);
      hub.transmit_message(i_msg);
    }
    // W 1F09: Set System Mode
    else if (opcode == 0x1F09 && msg.len >= 1) {
      this->system_mode_ = msg.payload[0];

      // Broadcast new mode
      this->broadcast_sync(hub);
    }
  }
}

void MockEvohomeController::broadcast_sync(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  uint8_t payload[3] = {this->system_mode_, 0x07, 0xD0};
  RamsesMessage msg = RamsesMessageBuilder::info()
                          .from(my_addr)
                          .addr2(my_addr)
                          .opcode(0x1F09)
                          .payload(payload, 3);
  hub.transmit_message(msg);
}

void MockEvohomeController::broadcast_temperatures(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  size_t count = std::min(this->zones_.size(), (size_t)8);
  std::vector<uint8_t> payload(count * 3);
  for (size_t i = 0; i < count; i++) {
    const auto &z = this->zones_[i];
    payload[i * 3 + 0] = z.index;
    uint16_t t_raw = (uint16_t)(z.current_temp * 100.0f);
    payload[i * 3 + 1] = (t_raw >> 8) & 0xFF;
    payload[i * 3 + 2] = t_raw & 0xFF;
  }
  RamsesMessage msg = RamsesMessageBuilder::info()
                          .from(my_addr)
                          .addr2(my_addr)
                          .opcode(0x30C9)
                          .payload(payload);
  hub.transmit_message(msg);
}

void MockEvohomeController::broadcast_setpoints(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  for (const auto &z : this->zones_) {
    uint16_t sp_raw = (uint16_t)(z.target_setpoint * 100.0f);
    uint8_t payload[3] = {z.index, static_cast<uint8_t>((sp_raw >> 8) & 0xFF),
                          static_cast<uint8_t>(sp_raw & 0xFF)};
    RamsesMessage msg = RamsesMessageBuilder::info()
                            .from(my_addr)
                            .addr2(my_addr)
                            .opcode(0x2309)
                            .payload(payload, 3);
    hub.transmit_message(msg);
  }
}

// ----------------------------------------------------------------------
// MockMvhrVentilator Implementation
// ----------------------------------------------------------------------
MockMvhrVentilator::MockMvhrVentilator(std::string device_id,
                                       MockHvacScheme scheme)
    : MockRamsesDevice(std::move(device_id)), scheme_(scheme) {
  switch (scheme) {
  case MOCK_SCHEME_ORCON:
    this->oem_code_ = 0x67;
    break;
  case MOCK_SCHEME_ITHO:
    this->oem_code_ = 0x08;
    break;
  case MOCK_SCHEME_VASCO:
    this->oem_code_ = 0x13;
    break;
  case MOCK_SCHEME_ZEHNDER:
    this->oem_code_ = 0x02;
    break;
  }
}

void MockMvhrVentilator::handle_incoming(const RamsesMessage &msg,
                                         MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  RamsesAddress src_addr = RamsesAddress::from_bytes(msg.addr[0]);
  RamsesAddress dst_addr = RamsesAddress::from_bytes(msg.addr[1]);
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  // Handle Query (RQ) to Fan
  if (msg.type == RAMSES_MSG_RQ && dst_addr.to_string() == this->device_id_) {
    // RQ 10E0: Device Info / OEM query
    if (opcode == 0x10E0) {
      uint8_t payload[38]{0};
      payload[0] = 0x00;
      payload[6] = this->oem_code_;
      RamsesMessage rp = RamsesMessageBuilder::reply()
                             .from(my_addr)
                             .to(src_addr)
                             .opcode(0x10E0)
                             .payload(payload, 38);
      hub.transmit_message(rp);
    }
    // RQ 22F1: Fan Status Query
    else if (opcode == 0x22F1) {
      uint8_t payload[3] = {0x00, this->fan_mode_, 0xFF};
      RamsesMessage rp = RamsesMessageBuilder::reply()
                             .from(my_addr)
                             .to(src_addr)
                             .opcode(0x22F1)
                             .payload(payload, 3);
      hub.transmit_message(rp);
    }
    // RQ 10D0: Filter Life Query
    else if (opcode == 0x10D0) {
      uint8_t days =
          (uint8_t)std::min((uint16_t)254, this->filter_days_remaining_);
      uint8_t payload[6] = {0x00, days, days, 0xC8, 0x00, 0x00};
      RamsesMessage rp = RamsesMessageBuilder::reply()
                             .from(my_addr)
                             .to(src_addr)
                             .opcode(0x10D0)
                             .payload(payload, 6);
      hub.transmit_message(rp);
    }
  }
  // Handle Write (W) from Remote / Gateway (Set Fan Speed)
  else if (msg.type == RAMSES_MSG_W &&
           (dst_addr.to_string() == this->device_id_ || !dst_addr.is_valid)) {
    if ((opcode == 0x22F1 || opcode == 0x22F3) && msg.len >= 2) {
      this->fan_mode_ = msg.payload[1];

      // Broadcast update
      this->broadcast_status(hub);
    }
  }
}

void MockMvhrVentilator::broadcast_status(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  uint8_t payload[3] = {0x00, this->fan_mode_, 0xFF};
  RamsesMessage msg = RamsesMessageBuilder::info()
                          .from(my_addr)
                          .addr2(my_addr)
                          .opcode(0x22F1)
                          .payload(payload, 3);
  hub.transmit_message(msg);
}

// ----------------------------------------------------------------------
// MockTrv Implementation
// ----------------------------------------------------------------------
MockTrv::MockTrv(std::string device_id)
    : MockRamsesDevice(std::move(device_id)) {}

void MockTrv::handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) {
  // TRVs primarily broadcast periodic telemetry
}

void MockTrv::broadcast_telemetry(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);

  uint8_t demand_p[2] = {0x00, static_cast<uint8_t>(this->demand_ * 200.0f)};
  RamsesMessage demand_msg = RamsesMessageBuilder::info()
                                 .from(my_addr)
                                 .addr2(my_addr)
                                 .opcode(0x3150)
                                 .payload(demand_p, 2);
  hub.transmit_message(demand_msg);

  uint8_t bat_p[3] = {0x00, this->battery_pct_, 0x01};
  RamsesMessage bat_msg = RamsesMessageBuilder::info()
                              .from(my_addr)
                              .addr2(my_addr)
                              .opcode(0x1060)
                              .payload(bat_p, 3);
  hub.transmit_message(bat_msg);
}

// ----------------------------------------------------------------------
// MockOpenThermBridge Implementation
// ----------------------------------------------------------------------
MockOpenThermBridge::MockOpenThermBridge(std::string device_id)
    : MockRamsesDevice(std::move(device_id)) {}

void MockOpenThermBridge::handle_incoming(const RamsesMessage &msg,
                                          MockRamsesEsp &hub) {}

void MockOpenThermBridge::broadcast_telemetry(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);

  uint8_t ot_p[5] = {
      0x00, static_cast<uint8_t>(this->flame_active_ ? 0x08 : 0x00), 0x00,
      static_cast<uint8_t>(this->modulation_pct_ * 2.0f), 0x00};
  RamsesMessage msg = RamsesMessageBuilder::info()
                          .from(my_addr)
                          .addr2(my_addr)
                          .opcode(0x3220)
                          .payload(ot_p, 5);
  hub.transmit_message(msg);
}

// ----------------------------------------------------------------------
// Universal Mock Ramses Hub Implementation
// ----------------------------------------------------------------------
MockRamsesEsp::MockRamsesEsp() = default;

void MockRamsesEsp::register_device(std::shared_ptr<MockRamsesDevice> dev) {
  if (dev) {
    this->devices_[dev->get_device_id()] = dev;
  }
}

std::shared_ptr<MockRamsesDevice>
MockRamsesEsp::get_device(const std::string &device_id) const {
  auto it = this->devices_.find(device_id);
  if (it != this->devices_.end()) {
    return it->second;
  }
  return nullptr;
}

void MockRamsesEsp::inject_message(const RamsesMessage &msg) {
  // Deliver incoming message to all simulated devices
  for (auto &pair : this->devices_) {
    pair.second->handle_incoming(msg, *this);
  }
}

void MockRamsesEsp::inject_hgi80(const std::string &hgi80_line) {
  RamsesMessage msg;
  if (msg.from_hgi80(hgi80_line)) {
    this->inject_message(msg);
  }
}

void MockRamsesEsp::transmit_message(const RamsesMessage &msg) {
  std::string hgi80 = msg.to_hgi80();
  this->tx_log_.push_back(hgi80);

  if (this->callback_) {
    this->callback_(msg, hgi80);
  }

  // Also deliver to local devices (simulating broadcast over air)
  for (auto &pair : this->devices_) {
    pair.second->handle_incoming(msg, *this);
  }
}

void MockRamsesEsp::tick(uint32_t now_ms) {
  for (auto &pair : this->devices_) {
    pair.second->tick(now_ms, *this);
  }
}

} // namespace ramses_esp
} // namespace esphome
