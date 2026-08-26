#include "ramses_frame.h"
#include "esphome/core/log.h"
#include "ramses_codec.h"
#include <ctime>
#include <sys/time.h>
#if __has_include("esp_timer.h")
#include "esp_timer.h"
#endif

static const char *TAG = "ramses_esp.frame";

#define RAMSES_SYNC_WORD 0x00335553
#define RAMSES_TRAILER 0x35

namespace esphome {
namespace ramses_esp {

enum MsgParseState {
  STATE_HDR = 0,
  STATE_ADDR0,
  STATE_ADDR1,
  STATE_ADDR2,
  STATE_PARAM0,
  STATE_PARAM1,
  STATE_OPCODE,
  STATE_LEN,
  STATE_PAYLOAD,
  STATE_CHECKSUM,
  STATE_DONE
};

bool RamsesFrameHandler::init(uart_port_t uart_num, gpio_num_t gdo0_pin,
                              gpio_num_t gdo2_pin, CC1101Driver *cc1101) {
  this->uart_num_ = uart_num;
  this->gdo0_pin_ = gdo0_pin;
  this->gdo2_pin_ = gdo2_pin;
  this->cc1101_ = cc1101;

  uart_config_t uart_config = {
      .baud_rate = 38400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
  };

  esp_err_t ret = uart_param_config(this->uart_num_, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "uart_param_config failed: %d", ret);
    return false;
  }

  // Set UART pins: TX = gdo0_pin_ (CC1101 TX In), RX = gdo2_pin_ (CC1101 RX
  // Out)
  ret = uart_set_pin(this->uart_num_, this->gdo0_pin_, this->gdo2_pin_,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "uart_set_pin failed: %d", ret);
    return false;
  }

  ret = uart_driver_install(this->uart_num_, 512, 0, 16, &this->uart_queue_, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "uart_driver_install failed: %d", ret);
    return false;
  }

  uart_set_rx_full_threshold(this->uart_num_, 1);
  uart_set_rx_timeout(this->uart_num_, 2);

  this->rx_enable();
  ESP_LOGI(TAG, "UART%d initialized: RX=GPIO%d (GDO2), TX=GPIO%d (GDO0)",
           this->uart_num_, this->gdo2_pin_, this->gdo0_pin_);
  return true;
}

void RamsesFrameHandler::rx_enable() {
  uart_flush_input(this->uart_num_);
  uart_enable_rx_intr(this->uart_num_);
  this->rx_state_ = FRM_RX_IDLE;
  this->reset_rx();
}

void RamsesFrameHandler::rx_disable() {
  uart_disable_rx_intr(this->uart_num_);
  this->rx_state_ = FRM_RX_OFF;
}

void RamsesFrameHandler::rx_flush() {
  if (this->uart_queue_ != nullptr) {
    UBaseType_t n = uxQueueMessagesWaiting(this->uart_queue_);
    uart_event_t event;
    for (UBaseType_t i = 0; i < n; i++) {
      xQueueReceive(this->uart_queue_, &event, 0);
    }
  }
  uart_flush_input(this->uart_num_);
}

void RamsesFrameHandler::reset_rx() {
  this->sync_buffer_ = 0;
  this->rx_raw_count_ = 0;
  this->rx_msg_count_ = 0;
  this->rx_msg_byte_ = 0;
  this->nibble_count_ = 0;
  this->msg_parse_state_ = STATE_HDR;
  this->msg_field_count_ = 0;
  this->current_msg_.reset();
}

void RamsesFrameHandler::work() {
  if (this->rx_state_ == FRM_RX_OFF)
    return;

  // Drain all available bytes from UART FIFO directly
  uint8_t buf[128];
  int bytes_read;
  while ((bytes_read = uart_read_bytes(this->uart_num_, buf, sizeof(buf), 0)) >
         0) {
    for (int i = 0; i < bytes_read; i++) {
      this->process_rx_byte(buf[i]);
    }
  }

  // Drain UART event queue to prevent queue overflow
  if (this->uart_queue_ != nullptr) {
    uart_event_t event;
    while (xQueueReceive(this->uart_queue_, &event, 0) == pdTRUE) {
      if (event.type == UART_BUFFER_FULL || event.type == UART_FIFO_OVF) {
        uart_flush_input(this->uart_num_);
        xQueueReset(this->uart_queue_);
        this->rx_state_ = FRM_RX_IDLE;
        this->reset_rx();
      }
    }
  }
}

static uint8_t next_state_after_hdr(uint8_t fields) {
  if (fields & RAMSES_F_ADDR0)
    return STATE_ADDR0;
  if (fields & RAMSES_F_ADDR1)
    return STATE_ADDR1;
  if (fields & RAMSES_F_ADDR2)
    return STATE_ADDR2;
  if (fields & RAMSES_F_PARAM0)
    return STATE_PARAM0;
  if (fields & RAMSES_F_PARAM1)
    return STATE_PARAM1;
  return STATE_OPCODE;
}

static uint8_t next_state_after_addr(uint8_t current_addr_state,
                                     uint8_t fields) {
  if (current_addr_state == STATE_ADDR0) {
    if (fields & RAMSES_F_ADDR1)
      return STATE_ADDR1;
    if (fields & RAMSES_F_ADDR2)
      return STATE_ADDR2;
  } else if (current_addr_state == STATE_ADDR1) {
    if (fields & RAMSES_F_ADDR2)
      return STATE_ADDR2;
  }
  if (fields & RAMSES_F_PARAM0)
    return STATE_PARAM0;
  if (fields & RAMSES_F_PARAM1)
    return STATE_PARAM1;
  return STATE_OPCODE;
}

void RamsesFrameHandler::process_rx_byte(uint8_t b) {
  switch (this->rx_state_) {
  case FRM_RX_OFF:
    break;

  case FRM_RX_IDLE:
  case FRM_RX_SYNCH:
    this->sync_buffer_ = (this->sync_buffer_ << 8) | b;
    if (this->sync_buffer_ == RAMSES_SYNC_WORD) {
      this->rx_state_ = FRM_RX_MESSAGE;
      this->reset_rx();
    }
    break;

  case FRM_RX_MESSAGE:
    if (b == RAMSES_TRAILER) {
      if (this->current_msg_.is_valid()) {
        this->handle_rx_done();
      }
      this->rx_state_ = FRM_RX_IDLE;
      this->reset_rx();
      return;
    }

    this->rx_raw_count_++;
    if (this->rx_raw_count_ >= RAMSES_MAX_RAW || !manchester_code_valid(b)) {
      if (this->current_msg_.is_valid()) {
        this->handle_rx_done();
      }
      this->rx_state_ = FRM_RX_IDLE;
      this->reset_rx();
      return;
    }

    this->rx_msg_byte_ = (this->rx_msg_byte_ << 4) | manchester_decode(b);
    this->nibble_count_ = 1 - this->nibble_count_;

    if (this->nibble_count_ == 0) {
      uint8_t byte = this->rx_msg_byte_;
      this->rx_msg_count_++;

      switch (this->msg_parse_state_) {
      case STATE_HDR:
        this->current_msg_.fields = ramses_decode_header(byte);
        this->current_msg_.type =
            static_cast<RamsesMsgType>((byte & HDR_T_MASK) >> HDR_T_SHIFT);
        this->msg_parse_state_ =
            next_state_after_hdr(this->current_msg_.fields);
        this->msg_field_count_ = 0;
        break;

      case STATE_ADDR0:
      case STATE_ADDR1:
      case STATE_ADDR2: {
        uint8_t addr_idx = this->msg_parse_state_ - STATE_ADDR0;
        this->current_msg_.addr[addr_idx][this->msg_field_count_++] = byte;
        if (this->msg_field_count_ == 3) {
          this->current_msg_.rx_fields |= (RAMSES_F_ADDR0 << addr_idx);
          this->msg_parse_state_ = next_state_after_addr(
              this->msg_parse_state_, this->current_msg_.fields);
          this->msg_field_count_ = 0;
        }
        break;
      }

      case STATE_PARAM0:
        this->current_msg_.param[0] = byte;
        this->current_msg_.rx_fields |= RAMSES_F_PARAM0;
        this->msg_parse_state_ = (this->current_msg_.fields & RAMSES_F_PARAM1)
                                     ? STATE_PARAM1
                                     : STATE_OPCODE;
        break;

      case STATE_PARAM1:
        this->current_msg_.param[1] = byte;
        this->current_msg_.rx_fields |= RAMSES_F_PARAM1;
        this->msg_parse_state_ = STATE_OPCODE;
        this->msg_field_count_ = 0;
        break;

      case STATE_OPCODE:
        this->current_msg_.opcode[this->msg_field_count_++] = byte;
        if (this->msg_field_count_ == 2) {
          this->current_msg_.rx_fields |= RAMSES_F_OPCODE;
          this->msg_parse_state_ = STATE_LEN;
          this->msg_field_count_ = 0;
        }
        break;

      case STATE_LEN:
        this->current_msg_.len = byte;
        this->current_msg_.rx_fields |= RAMSES_F_LEN;
        this->msg_field_count_ = 0;
        if (this->current_msg_.len == 0) {
          this->msg_parse_state_ = STATE_CHECKSUM;
        } else if (this->current_msg_.len > RAMSES_MAX_PAYLOAD) {
          this->rx_state_ = FRM_RX_ABORT;
        } else {
          this->msg_parse_state_ = STATE_PAYLOAD;
        }
        break;

      case STATE_PAYLOAD:
        this->current_msg_.payload[this->msg_field_count_++] = byte;
        this->current_msg_.n_payload = this->msg_field_count_;
        if (this->msg_field_count_ >= this->current_msg_.len) {
          this->msg_parse_state_ = STATE_CHECKSUM;
        }
        break;

      case STATE_CHECKSUM:
        this->current_msg_.csum = byte;
        this->msg_parse_state_ = STATE_DONE;
        if (this->current_msg_.is_valid()) {
          this->handle_rx_done();
          this->rx_state_ = FRM_RX_IDLE;
          this->reset_rx();
          return;
        }
        break;

      case STATE_DONE:
        break;
      }
    }

    break;

  case FRM_RX_DONE:
  case FRM_RX_ABORT:
    break;
  }
}

void RamsesFrameHandler::handle_rx_done() {
#ifdef USE_ESP_IDF
  this->last_rx_ms_ = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
#else
  this->last_rx_ms_ = 1000;
#endif
  if (this->cc1101_ != nullptr) {
    this->current_msg_.rssi = this->cc1101_->read_rssi();
  }

  // Generate ISO timestamp
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  struct tm *nowtm = localtime(&tv.tv_sec);
  if (nowtm != nullptr) {
    char ts_buf[24];
    strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%dT%H:%M:%S", nowtm);
    snprintf(this->current_msg_.timestamp, sizeof(this->current_msg_.timestamp),
             "%s.%03ld", ts_buf, tv.tv_usec / 1000);
  } else {
    this->current_msg_.timestamp[0] = '\0';
  }

  if (this->current_msg_.is_valid()) {
    std::string hgi80 = this->current_msg_.to_hgi80();
    ESP_LOGI(TAG, "RX: %s", hgi80.c_str());
    if (this->on_message_cb_ != nullptr) {
      this->on_message_cb_(this->current_msg_);
    }
  } else {
    ESP_LOGD(TAG, "Dropped invalid RAMSES frame (state=%d, fields=0x%02X)",
             this->msg_parse_state_, this->current_msg_.fields);
  }
}

} // namespace ramses_esp
} // namespace esphome
