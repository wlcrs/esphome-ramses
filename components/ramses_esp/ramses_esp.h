#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "cc1101_driver.h"
#include "ramses_frame.h"
#include "ramses_message.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <vector>
#include <string>

namespace esphome {
namespace ramses_esp {

class RamsesESPComponent : public Component {
 public:
  RamsesESPComponent() = default;

  void set_sck_pin(gpio_num_t pin) { this->sck_pin_ = pin; }
  void set_mosi_pin(gpio_num_t pin) { this->mosi_pin_ = pin; }
  void set_miso_pin(gpio_num_t pin) { this->miso_pin_ = pin; }
  void set_cs_pin(gpio_num_t pin) { this->cs_pin_ = pin; }
  void set_gdo0_pin(gpio_num_t pin) { this->gdo0_pin_ = pin; }
  void set_gdo2_pin(gpio_num_t pin) { this->gdo2_pin_ = pin; }
  void set_uart_num(uart_port_t uart_num) { this->uart_num_ = uart_num; }
  void set_port(uint16_t port) { this->port_ = port; }

  void add_on_message_callback(std::function<void(const std::string &)> callback) {
    this->on_message_callbacks_.push_back(callback);
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // High-level Actions
  bool send_hgi80_command(const std::string &cmd);

  // Multiplexer arbitration interface
  void pause();
  void resume();
  bool is_paused() const { return this->paused_; }

 protected:
  void start_tcp_server();
  void handle_tcp_clients();
  void broadcast_hgi80(const std::string &hgi80);
  void process_tx_queue();

  static void radio_task_trampoline(void *arg);
  void radio_task();

  gpio_num_t sck_pin_{GPIO_NUM_NC};
  gpio_num_t mosi_pin_{GPIO_NUM_NC};
  gpio_num_t miso_pin_{GPIO_NUM_NC};
  gpio_num_t cs_pin_{GPIO_NUM_NC};
  gpio_num_t gdo0_pin_{GPIO_NUM_NC};
  gpio_num_t gdo2_pin_{GPIO_NUM_NC};
  uart_port_t uart_num_{UART_NUM_1};
  uint16_t port_{6638};

  CC1101Driver cc1101_;
  RamsesFrameHandler frame_handler_;

  TaskHandle_t radio_task_handle_{nullptr};
  SemaphoreHandle_t radio_mutex_{nullptr};
  QueueHandle_t rx_msg_queue_{nullptr};
  QueueHandle_t tx_msg_queue_{nullptr};

  int server_fd_{-1};
  std::vector<int> client_fds_;

  bool paused_{false};

  std::vector<std::function<void(const std::string &)>> on_message_callbacks_;
};

template<typename... Ts>
class SendHgi80Action : public Action<Ts...> {
 public:
  SendHgi80Action(RamsesESPComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, command)

  void play(Ts... x) override {
    auto cmd = this->command_.value(x...);
    this->parent_->send_hgi80_command(cmd);
  }

 protected:
  RamsesESPComponent *parent_;
};

class RamsesMessageTrigger : public Trigger<std::string> {
 public:
  explicit RamsesMessageTrigger(RamsesESPComponent *parent) {
    parent->add_on_message_callback([this](const std::string &msg) {
      this->trigger(msg);
    });
  }
};

} // namespace ramses_esp
} // namespace esphome
