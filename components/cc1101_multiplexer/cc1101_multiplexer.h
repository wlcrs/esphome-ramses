#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cc1101/cc1101.h"
#include "components/ramses_esp/ramses_esp.h"
#include <vector>

namespace esphome {
namespace cc1101_multiplexer {

class CC1101MultiplexerComponent : public Component {
 public:
  CC1101MultiplexerComponent() = default;

  void set_ramses(ramses_esp::RamsesESPComponent *ramses) { this->ramses_ = ramses; }
  void set_cc1101(cc1101::CC1101Component *cc1101) { this->cc1101_ = cc1101; }
  void set_rx_window_ms(uint32_t ms) { this->rx_window_ms_ = ms; }

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool send_packet(const std::vector<uint8_t> &data);

 protected:
  ramses_esp::RamsesESPComponent *ramses_{nullptr};
  cc1101::CC1101Component *cc1101_{nullptr};
  uint32_t rx_window_ms_{75};

  bool waiting_reply_{false};
  uint32_t rx_window_end_time_{0};
};

template<typename... Ts>
class SendPacketAction : public Action<Ts...> {
 public:
  SendPacketAction(CC1101MultiplexerComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) override {
    auto d = this->data_.value(x...);
    this->parent_->send_packet(d);
  }

 protected:
  CC1101MultiplexerComponent *parent_;
};

} // namespace cc1101_multiplexer
} // namespace esphome
