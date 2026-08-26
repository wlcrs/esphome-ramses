# ESPHome RAMSES II Component (`ramses_esp`)

[![CI](https://github.com/wlcrs/esphome-ramses/actions/workflows/ci.yml/badge.svg)](https://github.com/wlcrs/esphome-ramses/actions/workflows/ci.yml)
[![ESPHome](https://img.shields.io/badge/ESPHome-External%20Component-blue.svg)](https://esphome.io)
[![Target](https://img.shields.io/badge/Target-ESP32%20%7C%20ESP32--C6-green.svg)](https://espressif.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An [ESPHome](https://esphome.io) external component for **ESP32** and **ESP32-C6** microcontrollers paired with a Texas Instruments **CC1101** (868.3 MHz) Sub-1GHz transceiver.

This component transforms your ESP32 into an **HGI80-compatible RAMSES II / Honeywell Evohome RF Gateway**, allowing seamless integration with Home Assistant's [`ramses_cc`](https://github.com/zxdavids/ramses_cc) / [`ramses_rf`](https://github.com/ramses-rf/ramses_rf) integration via a bidirectional TCP socket.

---

## Features

- **HGI80 Gateway Emulation**: Exposes a bidirectional ASCII stream server over TCP (default port `6638`), ready for Home Assistant's `ramses_cc` / `ramses_rf` (`socket://<esp-ip>:6638`).
- **Low-Latency Bitstream Framing**: Feeds demodulated 868.3 MHz 2-FSK (38.4 kBaud) data from CC1101 directly into an ESP32 hardware UART peripheral.
- **Real-Time Manchester Decoding**: Decodes Manchester frames in real time with preamble/sync word detection, frame extraction, and checksum validation.
- **ESP32 & ESP32-C6 Support**: Runs on both dual-core (ESP32/ESP32-S3) and single-core RISC-V (ESP32-C6) processors using the ESP-IDF framework.
- **ESPHome Automations**: Provides an `on_message` trigger and a `ramses_esp.send_hgi80` action for native ESPHome rules and logging.

---

## Origins & Credits

This project ports and builds upon the foundational work of several community members and projects:

- **Peter Price ([IndaloTech](https://github.com/IndaloTech/ramses_esp))**: Creator of the original standalone `ramses_esp` ESP-IDF firmware.
- **[IMMRMKW](https://github.com/IMMRMKW/ramses_esp/)**: Added ESP32-C6 single-core support, optimized CC1101 interrupt timing/FIFO thresholds, and created custom ESP32-C6 wireless boards ([Shop](https://elecram.com/products/esp32-c6-wifi-zigbee-to-855-925mhz-wireless?variant=52605637099853) / [Tindie](https://www.tindie.com/products/elecram/esp32-c6-wifizigbee-to-855-925mhz-wireless/)).
- **David Bonnes ([zxdavids](https://github.com/zxdavids/ramses_cc)) & the [ramses-rf](https://github.com/ramses-rf/ramses_rf) team**: Creators of the RAMSES RF protocol decoding library and Home Assistant integration.
- **The [ESPHome](https://esphome.io) team**: For the embedded framework and core abstractions.

---

## Hardware & Pinout

The CC1101 connects to the ESP32 via SPI (for register configuration and mode control) and routes its `GDO0` output to an ESP32 UART RX pin (for continuous demodulated bitstream reception).

### Typical ESP32-C6 Pinout
| CC1101 Pin | ESP32-C6 GPIO | Description |
|---|---|---|
| **CSN (CS)** | `GPIO 18` | SPI Chip Select |
| **SCK (CLK)** | `GPIO 6` | SPI Clock |
| **MOSI (SI)** | `GPIO 7` | SPI MOSI |
| **MISO (SO)** | `GPIO 2` | SPI MISO |
| **GDO0** | `GPIO 15` | Demodulated UART RX bitstream |
| **GDO2** | `GPIO 14` | Optional UART TX / FIFO status |

*(All pins are fully customizable in your YAML configuration)*

---

## Basic Configuration

To set up the RAMSES II gateway on your ESP32-C6:

```yaml
external_components:
  - source: github://wlcrs/esphome-ramses
    components: [ ramses_esp ]

esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

# Enable Home Assistant API, OTA, and Logging
api:
ota:
  - platform: esphome
logger:
  level: DEBUG

wifi:
  ssid: "MyWiFiNetwork"
  password: "MyWiFiPassword"

# RAMSES II Transceiver & HGI80 Gateway
ramses_esp:
  id: ramses_hub
  cs_pin: GPIO18
  sck_pin: GPIO6
  mosi_pin: GPIO7
  miso_pin: GPIO2
  gdo0_pin: GPIO15 # Connects to CC1101 GDO0
  gdo2_pin: GPIO14 # Connects to CC1101 GDO2 (optional)
  uart_num: 1      # ESP32 UART port (default: 1)
  port: 6638       # TCP port for Home Assistant (default: 6638)
  on_message:
    - lambda: |-
        ESP_LOGD("ramses", "Received HGI80: %s", x.c_str());
```

### Home Assistant Setup

In Home Assistant's `configuration.yaml`, point the [`ramses_cc`](https://github.com/zxdavids/ramses_cc) integration directly to your ESPHome device's IP address:

```yaml
ramses_cc:
  serial_port: "socket://192.168.1.100:6638"
  packet_log: /config/ramses_packets.log
  restore_cache: true
```

---

## MQTT Gateway Mode (`ramses-mqtt` Parity)

If you prefer to communicate with `ramses_rf` / `ramses_cc` over MQTT instead of a direct TCP socket, ESPHome can achieve **100% feature parity** with the standalone `ramses-mqtt` firmware.

This includes:
- **LWT & Availability**: `online`/`offline` published on `<root>/<device_id>`
- **Info Topics**: `<root>/<device_id>/info/firmware` and `version`
- **RX Packets**: Published to `<root>/<device_id>/rx` as JSON `{"msg": "<hgi80_frame>", "ts": "<timestamp>"}`
- **TX Packets**: Subscribed to `<root>/<device_id>/tx` expecting JSON `{"msg": "<hgi80_frame>"}`
- **CLI Commands**: Handshake response to `!V` on `<root>/<device_id>/cmd/cmd` -> `<root>/<device_id>/cmd/result`

Check out [`example-c6-mqtt.yaml`](example-c6-mqtt.yaml) for a complete, ready-to-flash MQTT configuration.

---

## Configuration Reference: `ramses_esp`

| Option | Type | Default | Description |
|---|---|---|---|
| **`cs_pin`** | **Required**, Pin | — | SPI Chip Select pin for the CC1101. |
| **`sck_pin`** | **Required**, Pin | — | SPI Clock pin. |
| **`mosi_pin`** | **Required**, Pin | — | SPI MOSI pin. |
| **`miso_pin`** | **Required**, Pin | — | SPI MISO pin. |
| **`gdo0_pin`** | **Required**, Pin | — | ESP32 UART RX pin connected to CC1101 GDO0. |
| **`gdo2_pin`** | Optional, Pin | — | ESP32 pin connected to CC1101 GDO2. |
| **`uart_num`** | Optional, Integer | `1` | ESP32 hardware UART peripheral number (0–2). |
| **`port`** | Optional, Integer | `6638` | TCP port for the HGI80 ASCII stream server. |
| **`on_message`** | Optional, Automation | — | Trigger fired whenever a valid RAMSES frame is received (passes the HGI80 string `x`). |

---

## Advanced: Radio Time-Sharing (`cc1101_multiplexer`)

### When to use the multiplexer?

A physical CC1101 radio cannot listen to two different bitrates or modulations at the exact same microsecond. 

If you have a **single CC1101 transceiver** on your board and want to use it **both** for continuous RAMSES II monitoring (868.3 MHz, 38.4 kbps Manchester) **and** to send commands to / receive replies from another 868 MHz device (such as an extractor hood, blinds, or custom FSK remote using ESPHome's standard `cc1101` component), you can use the companion `cc1101_multiplexer` component.

### How it works:
1. `ramses_esp` continuously monitors RAMSES II traffic 99.9% of the time.
2. When you send a command to your secondary device, `cc1101_multiplexer` temporarily pauses RAMSES listening, applies the standard ESPHome `cc1101` component's configuration (e.g. 64 kbps FSK with custom sync word and CRC), transmits the packet, and keeps the radio in RX mode for a brief window (`rx_window: 75ms`) so any reply triggers standard `cc1101.on_packet:`.
3. When the window expires, RAMSES monitoring is automatically resumed. The pause is under 80 ms, leaving heating system monitoring completely unaffected.

### Multi-Protocol Example

```yaml
external_components:
  - source: github://wlcrs/esphome-ramses
    components: [ ramses_esp, cc1101_multiplexer ]

# 1. Standard ESPHome CC1101 Component (e.g. for an extractor hood)
cc1101:
  id: hood_radio
  cs_pin: GPIO18
  gdo0_pin: GPIO15
  frequency: 868.3MHz
  modulation_type: 2-FSK
  symbol_rate: 64000
  packet_mode: true
  crc_enable: true
  sync0: 0x91
  sync1: 0xD3
  sync_mode: "16/16"
  on_packet:
    then:
      - lambda: |-
          ESP_LOGI("hood", "Received ACK packet: %d bytes", (int)x.size());

# 2. Pure RAMSES II Gateway
ramses_esp:
  id: ramses_hub
  cs_pin: GPIO18
  sck_pin: GPIO6
  mosi_pin: GPIO7
  miso_pin: GPIO2
  gdo0_pin: GPIO15
  gdo2_pin: GPIO14
  port: 6638

# 3. Radio Multiplexer / Time-Sharing Arbitrator
cc1101_multiplexer:
  id: radio_mux
  ramses_id: ramses_hub
  cc1101_id: hood_radio
  rx_window: 75ms

# 4. Transmit action in automations or buttons
button:
  - platform: template
    name: "Hood Fan Up"
    on_press:
      - cc1101_multiplexer.send_packet:
          id: radio_mux
          data: [ 0x0E, 0x46, 0x12, 0x34, 0x56, 0x78, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x27, 0x5A, 0x02 ]
```

---

## Testing & Verification

This project includes **native host C++ unit tests** for protocol logic and **ESPHome compile verification**:

### 1. Native C++ Unit Tests (Host Machine)
Tests the Manchester encoder/decoder, RAMSES address conversions, HGI80 parsing/formatting, checksum calculation, and raw radio frame generation on Linux/macOS without hardware:

```bash
cmake -B tests/build -S tests
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

### 2. ESPHome Compilation Test
Validates YAML schema generation and compiles the firmware with the ESP-IDF toolchain:

```bash
esphome compile example-c6.yaml
```

### 3. Continuous Integration
All PRs and commits are automatically verified via GitHub Actions against:
- The native C++ unit test suite (57 assertions).
- ESPHome YAML validation and ESP32-C6 ESP-IDF compilation.

---

## Contributing

Contributions, bug reports, and enhancements are welcome!

1. Fork the repository and create a feature branch (`git checkout -b feature/my-feature`).
2. Implement your changes and ensure code style is clean (`clang-format` for C++, `black`/`flake8` for Python).
3. Run the unit tests (`ctest --test-dir tests/build`) and add tests for any new protocol logic.
4. Verify compilation with `esphome compile example-c6.yaml`.
5. Open a Pull Request.

---

## License

This project is licensed under the [MIT License](LICENSE).  
Portions derived from `ramses_esp` are copyright &copy; 2023–2025 Peter Price & IMMRMKW.
