#include "cc1101_driver.h"
#include "esphome/core/log.h"
#include "esp_rom_sys.h"
#include <cstring>
#include <cmath>

static const char *TAG = "ramses_esp.cc1101";

#define delayMicroseconds(us) esp_rom_delay_us(us)

namespace esphome {
namespace ramses_esp {

static const uint8_t CC_RAMSES_CFG[CC_PARAM_MAX] = {
    0x0D, // CC_IOCFG2   GDO2- RX data
    0x2E, // CC_IOCFG1   GDO1- not used
    0x2E, // CC_IOCFG0   GDO0- TX data
    0x07, // CC_FIFOTHR  default
    0xD3, // CC_SYNC1    default
    0x91, // CC_SYNC0    default
    0x3D, // CC_PKTLEN   default
    0x04, // CC_PKTCTRL1 default
    0x31, // CC_PKTCTRL0 Asynchronous Serial, TX on GDO0, RX on GDOx
    0x00, // CC_ADDR     default
    0x00, // CC_CHANNR   default
    0x0F, // CC_FSCTRL1  default
    0x00, // CC_FSCTRL0  default
    0x21, // CC_FREQ2    868.3 MHz
    0x65, // CC_FREQ1
    0x6A, // CC_FREQ0
    0x6A, // CC_MDMCFG4
    0x83, // CC_MDMCFG3  DRATE_M=131 data rate=38,383.48Hz
    0x10, // CC_MDMCFG2  GFSK, No Sync Word
    0x22, // CC_MDMCFG1  FEC_EN=0, NUM_PREAMBLE=4, CHANSPC_E=2
    0xF8, // CC_MDMCFG0  Channel spacing 199.951 KHz
    0x50, // CC_DEVIATN
    0x07, // CC_MCSM2    default
    0x30, // CC_MCSM1    default
    0x18, // CC_MCSM0    Auto-calibrate on Idle to RX+TX
    0x16, // CC_FOCCFG   default
    0x6C, // CC_BSCFG    default
    0x43, // CC_AGCCTRL2
    0x40, // CC_AGCCTRL1 default
    0x91, // CC_AGCCTRL0 default
    0x87, // CC_WOREVT1  default
    0x6B, // CC_WOREVT0  default
    0xF8, // CC_WORCTRL  default
    0x56, // CC_FREND1   default
    0x10, // CC_FREND0   default
    0xE9, // CC_FSCAL3
    0x21, // CC_FSCAL2
    0x00, // CC_FSCAL1
    0x1F, // CC_FSCAL0
    0x41, // CC_RCCTRL1  default
    0x00, // CC_RCCTRL0  default
    0x59, // CC_FSTEST   default
    0x7F, // CC_PTEST    default
    0x3F, // CC_AGCTEST  default
    0x81, // CC_TEST2
    0x35, // CC_TEST1
    0x09  // CC_TEST0
};

static const uint8_t CC_DEFAULT_PA[CC_PA_MAX] = {
    0xC3, 0, 0, 0, 0, 0, 0, 0
};

bool CC1101Driver::init(spi_host_device_t host, gpio_num_t sck, gpio_num_t mosi, gpio_num_t miso, gpio_num_t cs) {
  this->host_ = host;
  this->sck_pin_ = sck;
  this->mosi_pin_ = mosi;
  this->miso_pin_ = miso;
  this->cs_pin_ = cs;

  this->spi_reset();

  // spi_bus_config_t buscfg = {};
  // buscfg.mosi_io_num = this->mosi_pin_;
  // buscfg.miso_io_num = this->miso_pin_;
  // buscfg.sclk_io_num = this->sck_pin_;
  // buscfg.quadwp_io_num = -1;
  // buscfg.quadhd_io_num = -1;
  // buscfg.max_transfer_sz = 64;
  spi_bus_config_t buscfg = {
      .mosi_io_num = this->mosi_pin_,
      .miso_io_num = this->miso_pin_,
      .sclk_io_num = this->sck_pin_,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 64,
  };
  esp_err_t ret = spi_bus_initialize(this->host_, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %d", ret);
    return false;
  }

  spi_device_interface_config_t devcfg = {};
  devcfg.mode = 0;
  devcfg.clock_speed_hz = 10000000; // 10 MHz
  devcfg.spics_io_num = this->cs_pin_;
  devcfg.flags = SPI_DEVICE_NO_DUMMY;
  devcfg.queue_size = 7;

  ret = spi_bus_add_device(this->host_, &devcfg, &this->spi_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_add_device failed: %d", ret);
    return false;
  }

  this->strobe(CC_SRES);
  delayMicroseconds(500);

  this->apply_ramses_config();
  return true;
}

void CC1101Driver::spi_reset() {
  gpio_reset_pin(this->cs_pin_);
  gpio_set_direction(this->cs_pin_, GPIO_MODE_OUTPUT);
  gpio_set_level(this->cs_pin_, 1);
  delayMicroseconds(1);
  gpio_set_level(this->cs_pin_, 0);
  delayMicroseconds(10);
  gpio_set_level(this->cs_pin_, 1);
  delayMicroseconds(41);
}

bool CC1101Driver::spi_write_bytes(uint8_t *status, const uint8_t *data, size_t len) {
  if (len == 0 || this->spi_handle_ == nullptr) return false;
  spi_transaction_t t = {
      .length = len * 8,
      .tx_buffer = data,
      .rx_buffer = status,
  };
  return spi_device_transmit(this->spi_handle_, &t) == ESP_OK;
}

bool CC1101Driver::spi_read_bytes(uint8_t *rx_data, const uint8_t *tx_data, size_t len) {
  if (len == 0 || this->spi_handle_ == nullptr) return false;
  spi_transaction_t t = {
      .length = len * 8,
      .tx_buffer = tx_data,
      .rx_buffer = rx_data,
  };
  return spi_device_transmit(this->spi_handle_, &t) == ESP_OK;
}

uint8_t CC1101Driver::read_reg(uint8_t addr) {
  uint8_t tx[2] = {static_cast<uint8_t>(addr | CC_READ), 0};
  uint8_t rx[2] = {0, 0};
  this->spi_read_bytes(rx, tx, 2);
  return rx[1];
}

uint8_t CC1101Driver::write_reg(uint8_t addr, uint8_t val) {
  uint8_t tx[2] = {addr, val};
  uint8_t status = 0;
  this->spi_write_bytes(&status, tx, 2);
  return status;
}

uint8_t CC1101Driver::strobe(uint8_t cmd) {
  uint8_t status = 0;
  this->spi_write_bytes(&status, &cmd, 1);
  return status;
}

uint8_t CC1101Driver::write_fifo(uint8_t b) {
  uint8_t res = this->write_reg(CC_FIFO, b);
  return res & 0x0F;
}

void CC1101Driver::write_fifo_burst(const uint8_t *data, size_t len) {
  if (len == 0) return;
  std::vector<uint8_t> buf(len + 1);
  buf[0] = CC_FIFO | CC_BURST;
  memcpy(buf.data() + 1, data, len);
  uint8_t status = 0;
  this->spi_write_bytes(&status, buf.data(), buf.size());
}

void CC1101Driver::enter_idle_mode() {
  while (CC_STATE(this->strobe(CC_SIDLE)) != CC_STATE_IDLE) {
    delayMicroseconds(10);
  }
}

void CC1101Driver::enter_rx_mode() {
  this->enter_idle_mode();
  this->write_reg(CC_IOCFG0, 0x2E);   // GDO0 not needed / async
  this->write_reg(CC_PKTCTRL0, 0x32); // Asynchronous, infinite packet
  this->strobe(CC_SFRX);
  while (CC_STATE(this->strobe(CC_SRX)) != CC_STATE_RX) {
    delayMicroseconds(10);
  }
}

void CC1101Driver::enter_tx_mode() {
  this->enter_idle_mode();
  this->write_reg(CC_PKTCTRL0, 0x02); // Fifo mode, infinite packet
  this->write_reg(CC_IOCFG0, 0x03);   // Falling edge, TX Fifo low
  this->strobe(CC_SFTX);
  while (CC_STATE(this->strobe(CC_STX)) != CC_STATE_TX) {
    delayMicroseconds(10);
  }
}

void CC1101Driver::fifo_end() {
  this->write_reg(CC_IOCFG0, 0x05); // Rising edge, TX Fifo empty
}

uint8_t CC1101Driver::read_rssi() {
  int8_t rssi = static_cast<int8_t>(this->read_reg(CC_RSSI));
  rssi = rssi / 2 - 74;
  return static_cast<uint8_t>(-rssi);
}

void CC1101Driver::apply_ramses_config() {
  this->enter_idle_mode();
  for (uint8_t i = 0; i < CC_PARAM_MAX; i++) {
    this->write_reg(i, CC_RAMSES_CFG[i]);
  }
  // TX Fifo Threshold 17
  this->write_reg(CC_FIFOTHR, (CC_RAMSES_CFG[CC_FIFOTHR] & 0xF0) + 11);
  for (uint8_t i = 0; i < CC_PA_MAX; i++) {
    this->write_reg(CC_PATABLE, CC_DEFAULT_PA[i]);
  }
  this->enter_rx_mode();
  ESP_LOGI(TAG, "CC1101 configured for RAMSES II RX (868.3 MHz)");
}

void CC1101Driver::apply_custom_tx_config(const CustomTxConfig &cfg) {
  this->enter_idle_mode();

  // 1. Calculate frequency registers: F_carrier = (F_xosc / 2^16) * FREQ
  // FREQ = freq_hz * 65536 / 26000000
  uint64_t freq_reg = ((uint64_t)cfg.frequency * 65536ULL) / 26000000ULL;
  this->write_reg(CC_FREQ2, (freq_reg >> 16) & 0xFF);
  this->write_reg(CC_FREQ1, (freq_reg >> 8) & 0xFF);
  this->write_reg(CC_FREQ0, freq_reg & 0xFF);

  // 2. Calculate symbol rate registers: DRATE = ((256 + DRATE_M) * 2^DRATE_E / 2^28) * 26 MHz
  int drate_e = static_cast<int>(std::floor(std::log2(cfg.symbol_rate * 1048576.0f / 26000000.0f)));
  if (drate_e < 0) drate_e = 0;
  if (drate_e > 15) drate_e = 15;
  int drate_m = static_cast<int>(std::round((cfg.symbol_rate * 268435456.0f) / (26000000.0f * std::pow(2.0f, drate_e)) - 256.0f));
  if (drate_m < 0) drate_m = 0;
  if (drate_m > 255) drate_m = 255;

  uint8_t mdmcfg4 = (CC_RAMSES_CFG[CC_MDMCFG4] & 0xF0) | (drate_e & 0x0F);
  this->write_reg(CC_MDMCFG4, mdmcfg4);
  this->write_reg(CC_MDMCFG3, drate_m & 0xFF);

  // 3. Sync word & modulation (2-FSK, 16/16 sync word detection/insertion)
  this->write_reg(CC_SYNC1, cfg.sync1);
  this->write_reg(CC_SYNC0, cfg.sync0);
  this->write_reg(CC_MDMCFG2, 0x03); // 2-FSK, 16/16 sync bits, no Manchester

  // 4. Packet Control
  uint8_t pktctrl0 = (cfg.crc_enable ? 0x04 : 0x00) | (cfg.packet_length == 0 ? 0x01 : 0x00);
  this->write_reg(CC_PKTCTRL0, pktctrl0);
  this->write_reg(CC_PKTCTRL1, 0x04); // Append status (RSSI/LQI)
  this->write_reg(CC_PKTLEN, cfg.packet_length == 0 ? 0xFF : cfg.packet_length);

  // GDO0 asserts on sync, deasserts on end of packet
  this->write_reg(CC_IOCFG0, 0x06);

  this->strobe(CC_SFTX);
  ESP_LOGV(TAG, "CC1101 custom TX profile applied (Freq: %lu Hz, Rate: %.1f Baud, Sync: %02X%02X)",
           (unsigned long)cfg.frequency, cfg.symbol_rate, cfg.sync1, cfg.sync0);
}

} // namespace ramses_esp
} // namespace esphome
