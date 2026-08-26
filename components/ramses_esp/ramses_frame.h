#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "ramses_message.h"
#include "cc1101_driver.h"

namespace esphome {
namespace ramses_esp {

enum FrameRxState {
  FRM_RX_OFF,
  FRM_RX_IDLE,
  FRM_RX_SYNCH,
  FRM_RX_MESSAGE,
  FRM_RX_DONE,
  FRM_RX_ABORT
};

class RamsesFrameHandler {
 public:
  RamsesFrameHandler() = default;

  bool init(uart_port_t uart_num, gpio_num_t gdo0_pin, gpio_num_t gdo2_pin, CC1101Driver *cc1101);
  void set_on_message_callback(std::function<void(const RamsesMessage &)> cb) {
    this->on_message_cb_ = cb;
  }

  void rx_enable();
  void rx_disable();
  void rx_flush();

  void work();

 protected:
  void process_rx_byte(uint8_t b);
  void handle_rx_done();
  void reset_rx();

  uart_port_t uart_num_{UART_NUM_1};
  gpio_num_t gdo0_pin_{GPIO_NUM_NC};
  gpio_num_t gdo2_pin_{GPIO_NUM_NC};
  CC1101Driver *cc1101_{nullptr};
  QueueHandle_t uart_queue_{nullptr};

  FrameRxState rx_state_{FRM_RX_OFF};
  uint32_t sync_buffer_{0};
  uint8_t rx_raw_count_{0};
  uint8_t rx_msg_count_{0};
  uint8_t rx_msg_byte_{0};
  uint8_t nibble_count_{0};

  RamsesMessage current_msg_;
  uint8_t msg_parse_state_{0};
  uint8_t msg_field_count_{0};

  std::function<void(const RamsesMessage &)> on_message_cb_{nullptr};
};

} // namespace ramses_esp
} // namespace esphome
