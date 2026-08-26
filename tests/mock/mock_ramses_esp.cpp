#include "mock_ramses_esp.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace esphome {
namespace ramses_esp {

// ----------------------------------------------------------------------
// MockEvohomeController Implementation
// ----------------------------------------------------------------------
MockEvohomeController::MockEvohomeController(std::string controller_id)
    : MockRamsesDevice(std::move(controller_id)) {}

void MockEvohomeController::add_zone(uint8_t index, const std::string &name, float current_temp, float setpoint) {
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

void MockEvohomeController::handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) {
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
          RamsesMessage rp;
          rp.type = RAMSES_MSG_RP;
          rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
          my_addr.to_bytes(rp.addr[0]);
          src_addr.to_bytes(rp.addr[1]);
          rp.opcode[0] = 0x00;
          rp.opcode[1] = 0x04;
          rp.len = rp.n_payload = 22;
          std::memset(rp.payload, 0, 22);
          rp.payload[0] = z.index;
          rp.payload[1] = 0x00;
          size_t name_len = std::min(z.name.length(), (size_t)20);
          std::memcpy(&rp.payload[2], z.name.data(), name_len);
          hub.transmit_message(rp);
          break;
        }
      }
    }
    // RQ 0005: Zone Structure Query
    else if (opcode == 0x0005) {
      RamsesMessage rp;
      rp.type = RAMSES_MSG_RP;
      rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
      my_addr.to_bytes(rp.addr[0]);
      src_addr.to_bytes(rp.addr[1]);
      rp.opcode[0] = 0x00;
      rp.opcode[1] = 0x05;
      rp.len = rp.n_payload = 4;
      rp.payload[0] = 0x00; // Zone type (Radiator / Heat)
      // Bitmask of active zones
      uint16_t mask = 0;
      for (const auto &z : this->zones_) {
        if (z.index < 16) mask |= (1 << z.index);
      }
      rp.payload[1] = (mask >> 8) & 0xFF;
      rp.payload[2] = mask & 0xFF;
      rp.payload[3] = (uint8_t)this->zones_.size();
      hub.transmit_message(rp);
    }
    // RQ 1F09: System Sync / Mode Query
    else if (opcode == 0x1F09) {
      RamsesMessage rp;
      rp.type = RAMSES_MSG_RP;
      rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
      my_addr.to_bytes(rp.addr[0]);
      src_addr.to_bytes(rp.addr[1]);
      rp.opcode[0] = 0x1F;
      rp.opcode[1] = 0x09;
      rp.len = rp.n_payload = 3;
      rp.payload[0] = this->system_mode_;
      rp.payload[1] = 0x07; // Remaining time MSB / flag
      rp.payload[2] = 0xD0; // Remaining time LSB
      hub.transmit_message(rp);
    }
    // RQ 2309: Setpoint Query
    else if (opcode == 0x2309 && msg.len >= 1) {
      uint8_t requested_zone = msg.payload[0];
      for (const auto &z : this->zones_) {
        if (z.index == requested_zone) {
          RamsesMessage rp;
          rp.type = RAMSES_MSG_RP;
          rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
          my_addr.to_bytes(rp.addr[0]);
          src_addr.to_bytes(rp.addr[1]);
          rp.opcode[0] = 0x23;
          rp.opcode[1] = 0x09;
          rp.len = rp.n_payload = 3;
          rp.payload[0] = z.index;
          uint16_t sp_raw = (uint16_t)(z.target_setpoint * 100.0f);
          rp.payload[1] = (sp_raw >> 8) & 0xFF;
          rp.payload[2] = sp_raw & 0xFF;
          hub.transmit_message(rp);
          break;
        }
      }
    }
  }
  // Handle Write (W) from Gateway (e.g. Set Setpoint or Set Mode)
  else if (msg.type == RAMSES_MSG_W && (dst_addr.to_string() == this->device_id_ || !dst_addr.is_valid)) {
    // W 2309: Set Zone Setpoint
    if (opcode == 0x2309 && msg.len >= 3) {
      uint8_t z_idx = msg.payload[0];
      uint16_t sp_raw = ((uint16_t)msg.payload[1] << 8) | msg.payload[2];
      float new_sp = sp_raw / 100.0f;
      this->set_zone_setpoint(z_idx, new_sp);

      // Echo update as broadcast I 2309
      RamsesMessage i_msg;
      i_msg.type = RAMSES_MSG_I;
      i_msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
      my_addr.to_bytes(i_msg.addr[0]);
      my_addr.to_bytes(i_msg.addr[2]);
      i_msg.opcode[0] = 0x23;
      i_msg.opcode[1] = 0x09;
      i_msg.len = i_msg.n_payload = 3;
      i_msg.payload[0] = z_idx;
      i_msg.payload[1] = msg.payload[1];
      i_msg.payload[2] = msg.payload[2];
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
  RamsesMessage msg;
  msg.type = RAMSES_MSG_I;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
  my_addr.to_bytes(msg.addr[0]);
  my_addr.to_bytes(msg.addr[2]);
  msg.opcode[0] = 0x1F;
  msg.opcode[1] = 0x09;
  msg.len = msg.n_payload = 3;
  msg.payload[0] = this->system_mode_;
  msg.payload[1] = 0x07;
  msg.payload[2] = 0xD0;
  hub.transmit_message(msg);
}

void MockEvohomeController::broadcast_temperatures(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  RamsesMessage msg;
  msg.type = RAMSES_MSG_I;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
  my_addr.to_bytes(msg.addr[0]);
  my_addr.to_bytes(msg.addr[2]);
  msg.opcode[0] = 0x30;
  msg.opcode[1] = 0xC9;

  // Packed multi-zone temperature payload: [zone_index, temp_high, temp_low] * N
  size_t count = std::min(this->zones_.size(), (size_t)8);
  msg.len = msg.n_payload = count * 3;
  for (size_t i = 0; i < count; i++) {
    const auto &z = this->zones_[i];
    msg.payload[i * 3 + 0] = z.index;
    uint16_t t_raw = (uint16_t)(z.current_temp * 100.0f);
    msg.payload[i * 3 + 1] = (t_raw >> 8) & 0xFF;
    msg.payload[i * 3 + 2] = t_raw & 0xFF;
  }
  hub.transmit_message(msg);
}

void MockEvohomeController::broadcast_setpoints(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  for (const auto &z : this->zones_) {
    RamsesMessage msg;
    msg.type = RAMSES_MSG_I;
    msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
    my_addr.to_bytes(msg.addr[0]);
    my_addr.to_bytes(msg.addr[2]);
    msg.opcode[0] = 0x23;
    msg.opcode[1] = 0x09;
    msg.len = msg.n_payload = 3;
    msg.payload[0] = z.index;
    uint16_t sp_raw = (uint16_t)(z.target_setpoint * 100.0f);
    msg.payload[1] = (sp_raw >> 8) & 0xFF;
    msg.payload[2] = sp_raw & 0xFF;
    hub.transmit_message(msg);
  }
}

// ----------------------------------------------------------------------
// MockMvhrVentilator Implementation
// ----------------------------------------------------------------------
MockMvhrVentilator::MockMvhrVentilator(std::string device_id, MockHvacScheme scheme)
    : MockRamsesDevice(std::move(device_id)), scheme_(scheme) {
  switch (scheme) {
    case MOCK_SCHEME_ORCON: this->oem_code_ = 0x67; break;
    case MOCK_SCHEME_ITHO:  this->oem_code_ = 0x08; break;
    case MOCK_SCHEME_VASCO: this->oem_code_ = 0x13; break;
    case MOCK_SCHEME_ZEHNDER: this->oem_code_ = 0x02; break;
  }
}

void MockMvhrVentilator::handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);
  RamsesAddress src_addr = RamsesAddress::from_bytes(msg.addr[0]);
  RamsesAddress dst_addr = RamsesAddress::from_bytes(msg.addr[1]);
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];

  // Handle Query (RQ) to Fan
  if (msg.type == RAMSES_MSG_RQ && dst_addr.to_string() == this->device_id_) {
    // RQ 10E0: Device Info / OEM query
    if (opcode == 0x10E0) {
      RamsesMessage rp;
      rp.type = RAMSES_MSG_RP;
      rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
      my_addr.to_bytes(rp.addr[0]);
      src_addr.to_bytes(rp.addr[1]);
      rp.opcode[0] = 0x10;
      rp.opcode[1] = 0xE0;
      rp.len = rp.n_payload = 38;
      std::memset(rp.payload, 0, 38);
      rp.payload[0] = 0x00;
      rp.payload[6] = this->oem_code_; // OEM signature byte
      hub.transmit_message(rp);
    }
    // RQ 22F1: Fan Status Query
    else if (opcode == 0x22F1) {
      RamsesMessage rp;
      rp.type = RAMSES_MSG_RP;
      rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
      my_addr.to_bytes(rp.addr[0]);
      src_addr.to_bytes(rp.addr[1]);
      rp.opcode[0] = 0x22;
      rp.opcode[1] = 0xF1;
      rp.len = rp.n_payload = 3;
      rp.payload[0] = 0x00;
      rp.payload[1] = this->fan_mode_;
      rp.payload[2] = 0xFF;
      hub.transmit_message(rp);
    }
    // RQ 10D0: Filter Life Query
    else if (opcode == 0x10D0) {
      RamsesMessage rp;
      rp.type = RAMSES_MSG_RP;
      rp.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR1;
      my_addr.to_bytes(rp.addr[0]);
      src_addr.to_bytes(rp.addr[1]);
      rp.opcode[0] = 0x10;
      rp.opcode[1] = 0xD0;
      rp.len = rp.n_payload = 6;
      rp.payload[0] = 0x00; // Header
      rp.payload[1] = (uint8_t)std::min((uint16_t)254, this->filter_days_remaining_); // Remaining days
      rp.payload[2] = (uint8_t)std::min((uint16_t)254, this->filter_days_remaining_); // Total lifetime days
      rp.payload[3] = 0xC8; // 200 = 100% remaining
      rp.payload[4] = 0x00;
      rp.payload[5] = 0x00;
      hub.transmit_message(rp);
    }
  }
  // Handle Write (W) from Remote / Gateway (Set Fan Speed)
  else if (msg.type == RAMSES_MSG_W && (dst_addr.to_string() == this->device_id_ || !dst_addr.is_valid)) {
    if ((opcode == 0x22F1 || opcode == 0x22F3) && msg.len >= 2) {
      this->fan_mode_ = msg.payload[1];

      // Broadcast update
      this->broadcast_status(hub);
    }
  }
}

void MockMvhrVentilator::broadcast_status(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);

  // Broadcast I 22F1 (Fan Mode)
  RamsesMessage msg;
  msg.type = RAMSES_MSG_I;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
  my_addr.to_bytes(msg.addr[0]);
  my_addr.to_bytes(msg.addr[2]);
  msg.opcode[0] = 0x22;
  msg.opcode[1] = 0xF1;
  msg.len = msg.n_payload = 3;
  msg.payload[0] = 0x00;
  msg.payload[1] = this->fan_mode_;
  msg.payload[2] = 0xFF;
  hub.transmit_message(msg);
}

// ----------------------------------------------------------------------
// MockTrv Implementation
// ----------------------------------------------------------------------
MockTrv::MockTrv(std::string device_id) : MockRamsesDevice(std::move(device_id)) {}

void MockTrv::handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) {
  // TRVs primarily broadcast periodic telemetry
}

void MockTrv::broadcast_telemetry(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);

  // Broadcast I 3150 (Heat demand: 0..200 = 0..100%)
  RamsesMessage demand_msg;
  demand_msg.type = RAMSES_MSG_I;
  demand_msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
  my_addr.to_bytes(demand_msg.addr[0]);
  my_addr.to_bytes(demand_msg.addr[2]);
  demand_msg.opcode[0] = 0x31;
  demand_msg.opcode[1] = 0x50;
  demand_msg.len = demand_msg.n_payload = 2;
  demand_msg.payload[0] = 0x00; // Domain / zone index
  demand_msg.payload[1] = (uint8_t)(this->demand_ * 200.0f);
  hub.transmit_message(demand_msg);

  // Broadcast I 1060 (Battery %: 0..100)
  RamsesMessage bat_msg;
  bat_msg.type = RAMSES_MSG_I;
  bat_msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
  my_addr.to_bytes(bat_msg.addr[0]);
  my_addr.to_bytes(bat_msg.addr[2]);
  bat_msg.opcode[0] = 0x10;
  bat_msg.opcode[1] = 0x60;
  bat_msg.len = bat_msg.n_payload = 3;
  bat_msg.payload[0] = 0x00;
  bat_msg.payload[1] = this->battery_pct_;
  bat_msg.payload[2] = 0x01; // Good battery flag
  hub.transmit_message(bat_msg);
}

// ----------------------------------------------------------------------
// MockOpenThermBridge Implementation
// ----------------------------------------------------------------------
MockOpenThermBridge::MockOpenThermBridge(std::string device_id) : MockRamsesDevice(std::move(device_id)) {}

void MockOpenThermBridge::handle_incoming(const RamsesMessage &msg, MockRamsesEsp &hub) {}

void MockOpenThermBridge::broadcast_telemetry(MockRamsesEsp &hub) {
  RamsesAddress my_addr = RamsesAddress::from_string(this->device_id_);

  // Broadcast I 3220 (OpenTherm Data)
  RamsesMessage msg;
  msg.type = RAMSES_MSG_I;
  msg.fields = RAMSES_F_ADDR0 | RAMSES_F_ADDR2;
  my_addr.to_bytes(msg.addr[0]);
  my_addr.to_bytes(msg.addr[2]);
  msg.opcode[0] = 0x32;
  msg.opcode[1] = 0x20;
  msg.len = msg.n_payload = 5;
  msg.payload[0] = 0x00; // Data ID 0x00: Status
  msg.payload[1] = this->flame_active_ ? 0x08 : 0x00; // Flame on bit
  msg.payload[2] = 0x00;
  msg.payload[3] = (uint8_t)(this->modulation_pct_ * 2.0f); // Modulation % (0..200)
  msg.payload[4] = 0x00;
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

std::shared_ptr<MockRamsesDevice> MockRamsesEsp::get_device(const std::string &device_id) const {
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
