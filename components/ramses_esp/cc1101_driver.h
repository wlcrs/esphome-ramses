#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "cc_const.h"

namespace esphome {
namespace ramses_esp {

struct CustomTxConfig {
  uint32_t frequency{868300000}; // Hz
  float symbol_rate{64000.0f};    // Baud
  uint8_t sync0{0x91};
  uint8_t sync1{0xD3};
  bool crc_enable{true};
  uint8_t packet_length{0};      // 0 = variable length
};

class CC1101Driver {
 public:
  CC1101Driver() = default;

  bool init(spi_host_device_t host, gpio_num_t sck, gpio_num_t mosi, gpio_num_t miso, gpio_num_t cs);

  uint8_t read_reg(uint8_t addr);
  uint8_t write_reg(uint8_t addr, uint8_t val);
  uint8_t strobe(uint8_t cmd);
  uint8_t write_fifo(uint8_t b);
  void write_fifo_burst(const uint8_t *data, size_t len);

  void enter_idle_mode();
  void enter_rx_mode();
  void enter_tx_mode();
  void fifo_end();
  uint8_t read_rssi();

  void apply_ramses_config();
  void apply_custom_tx_config(const CustomTxConfig &cfg);

 protected:
  void spi_reset();
  bool spi_write_bytes(uint8_t *status, const uint8_t *data, size_t len);
  bool spi_read_bytes(uint8_t *rx_data, const uint8_t *tx_data, size_t len);

  spi_device_handle_t spi_handle_{nullptr};
  spi_host_device_t host_{SPI2_HOST};
  gpio_num_t sck_pin_{GPIO_NUM_NC};
  gpio_num_t mosi_pin_{GPIO_NUM_NC};
  gpio_num_t miso_pin_{GPIO_NUM_NC};
  gpio_num_t cs_pin_{GPIO_NUM_NC};
};

} // namespace ramses_esp
} // namespace esphome
