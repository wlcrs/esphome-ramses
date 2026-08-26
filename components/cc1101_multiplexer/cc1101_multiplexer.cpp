#include "cc1101_multiplexer.h"
#include "esphome/core/log.h"

static const char *const TAG = "cc1101_multiplexer";

namespace esphome {
namespace cc1101_multiplexer {

bool CC1101MultiplexerComponent::send_packet(const std::vector<uint8_t> &data) {
  if (data.empty() || this->cc1101_ == nullptr) {
    return false;
  }

  ESP_LOGD(
      TAG,
      "Arbitrating radio: pausing RAMSES, transmitting packet (%d bytes)...",
      (int)data.size());

  // 1. Pause RAMSES background listener
  if (this->ramses_ != nullptr) {
    this->ramses_->pause();
  }

  // 2. Re-apply CC1101 component settings (frequency, baud, sync, packet mode)
  this->cc1101_->configure();

  // 3. Transmit packet via CC1101 component
  this->cc1101_->transmit_packet(data);

  // 4. Enter RX mode to capture reply / ACK
  this->cc1101_->begin_rx();
  this->waiting_reply_ = true;
  this->rx_window_end_time_ = millis() + this->rx_window_ms_;

  ESP_LOGV(TAG, "Packet sent. Listening for reply (window: %u ms)...",
           (unsigned)this->rx_window_ms_);
  return true;
}

void CC1101MultiplexerComponent::loop() {
  if (!this->waiting_reply_)
    return;

  // Let CC1101 component process any received packets (triggers on_packet:)
  if (this->cc1101_ != nullptr) {
    this->cc1101_->loop();
  }

  // Check if listening window has expired
  if (millis() >= this->rx_window_end_time_) {
    ESP_LOGD(TAG, "Reply listening window expired; restoring RAMSES...");
    this->waiting_reply_ = false;

    if (this->cc1101_ != nullptr) {
      this->cc1101_->set_idle();
    }

    if (this->ramses_ != nullptr) {
      this->ramses_->resume();
    }
  }
}

void CC1101MultiplexerComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CC1101 Radio Multiplexer:");
  ESP_LOGCONFIG(TAG, "  RX Listening Window: %u ms",
                (unsigned)this->rx_window_ms_);
}

} // namespace cc1101_multiplexer
} // namespace esphome
