#pragma once

#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include "esphome/core/component.h"
#include <optional>
#include <string>

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_esp {
class RamsesESPComponent;
}

namespace ramses_devices {

static inline bool address_matches(const ramses_esp::RamsesAddress &configured,
                                   const ramses_esp::RamsesMessage &msg) {
  if (!configured.is_valid)
    return true;
  ramses_esp::RamsesAddress src =
      ramses_esp::RamsesAddress::from_bytes(msg.addr[0]);
  if (src == configured)
    return true;
  if (msg.fields & RAMSES_F_ADDR2) {
    ramses_esp::RamsesAddress targ =
        ramses_esp::RamsesAddress::from_bytes(msg.addr[2]);
    if (targ == configured)
      return true;
  }
  return false;
}

class RamsesEntityBase {
public:
  virtual ~RamsesEntityBase() = default;

  void set_parent(ramses_esp::RamsesESPComponent *parent) {
    this->parent_ = parent;
  }

  void set_device_address(const std::string &addr) {
    this->device_address_ = ramses_esp::RamsesAddress::from_string(addr);
  }

  void set_device_address(const ramses_esp::RamsesAddress &addr) {
    this->device_address_ = addr;
  }

  const ramses_esp::RamsesAddress &get_device_address() const {
    return this->device_address_;
  }

  void on_message(const ramses_esp::RamsesMessage &msg) {
    if (this->matches(msg)) {
      this->handle_message(msg);
    }
  }

protected:
  ramses_esp::RamsesESPComponent *parent_{nullptr};
  ramses_esp::RamsesAddress device_address_;

  virtual bool matches(const ramses_esp::RamsesMessage &msg) const {
    return address_matches(this->device_address_, msg);
  }

  void setup_base(ramses_esp::RamsesESPComponent *parent) {
    this->parent_ = parent;
#ifdef USE_ESP_IDF
    if (this->parent_ != nullptr) {
      this->parent_->add_raw_message_callback(
          [this](const ramses_esp::RamsesMessage &msg) {
            if (this->matches(msg)) {
              this->handle_message(msg);
            }
          });
    }
#endif
  }

  void setup_base() { this->setup_base(this->parent_); }

  virtual void handle_message(const ramses_esp::RamsesMessage &msg) = 0;
};

} // namespace ramses_devices
} // namespace esphome
