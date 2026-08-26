# RAMSES ESP Testing & Parity Framework

This directory contains the automated test suite, mock hardware simulators, and dual-sided parity verification harness for `esphome-ramses`.

---

## 1. Why We Test This Way

The Honeywell RAMSES II protocol and multi-vendor MVHR ecosystem (Orcon, Itho, Vasco, Zehnder) have been reverse-engineered over many years in Python by the `ramses_rf` / `ramses_cc` community. 

When porting this functionality to high-performance, zero-heap C++ in ESPHome, we face two critical testing challenges:
1. **Physical RF Hardware Constraints**: We cannot rely on having physical Evohome controllers, TRVs, or MVHR units connected to CI runners.
2. **Behavioral Divergence Risk**: Subtle protocol edge cases (sentinel error values like `0x7FFF`, multi-zone array packing, fixed-point scalings, OEM byte signatures) could silently break if our C++ decoders disagree with standard `ramses_rf` decoding.

To solve both challenges, we use a **Dual-Sided Parity Testing Architecture**:

```
                         ┌────────────────────────────────────┐
                         │   Canonical Test Fixtures (JSON)   │
                         │   `tests/fixtures/parity_cases.json`│
                         └─────────────────┬──────────────────┘
                                           │ Input Packet (HGI80) & Expected Values
                  ┌────────────────────────┴────────────────────────┐
                  │                                                 │
                  ▼                                                 ▼
     ┌─────────────────────────┐                       ┌─────────────────────────┐
     │    C++ Decoder Suite    │                       │    Python ramses_rf     │
     │  `test_parity_cases`    │                       │  `test_parity_harness`  │
     └────────────┬────────────┘                       └────────────┬────────────┘
                  │ Decoded C++ Values                              │ Decoded Python Values
                  │                                                 │
                  └────────────────────────┬────────────────────────┘
                                           │
                                           ▼
                         ┌────────────────────────────────────┐
                         │   Assert 100% Differential Parity  │
                         └────────────────────────────────────┘
```

---

## 2. Directory Structure

* **`fixtures/parity_cases.json`**:  
  The single source of truth containing canonical test cases across all supported opcodes (`1F09`, `2309`, `30C9`, `0004`, `22F1`, `10E0`, `3150`, `1060`, `3220`, `10D0`). It specifies the raw HGI80 packet and the exact expected decoded fields.
* **`mock/mock_ramses_esp.h` / `mock_ramses_esp.cpp`**:  
  Universal simulation engine containing configurable virtual devices:
  * `MockEvohomeController`: Simulates multi-zone heating controllers with ASCII zone name queries (`RQ 0004`), structure queries (`RQ 0005`), setpoint queries (`RQ 2309`), and setpoint/mode writes (`W 2309`, `W 1F09`).
  * `MockMvhrVentilator`: Simulates MVHR units with configurable vendor profiles (Orcon, Itho, Vasco, Zehnder), OEM signatures (`RQ 10E0`), and fan speed commands (`W 22F1`).
  * `MockTrv`: Simulates radiator valves broadcasting heat demand (`0x3150`) and battery telemetry (`0x1060`).
  * `MockOpenThermBridge`: Simulates boiler bridges broadcasting modulation and status (`0x3220`).
* **`test_protocol.cpp`**:  
  Low-level unit tests for Manchester codecs, address parsing/formatting, and HGI80 framing.
* **`test_mock.cpp`**:  
  Integration tests verifying virtual device simulation and bidirectional query/reply flows.
* **`test_parity_cases.cpp`**:  
  C++ test runner that validates the parity fixture structure, parses each HGI80 frame, and verifies the currently covered semantic fields.
* **`test_parser_corpus.cpp`**:  
  Bulk corpus regression test runner that recursively ingests the local corpus of **170 `packet.log` files**. It reports candidate frames, invalid packets, per-opcode coverage, and known-opcode decode failures. Variant failures are intentionally visible until their decoders are implemented.
* **`test_devices_sensors.cpp`**:  
  Integration tests for all sensor and binary sensor types (`temperature`, `setpoint`, `co2`, `humidity`, `filter_alarm`, `flame_active`, `battery_low`, etc.).
* **`test_devices_climate.cpp`**:  
  Unit tests for multi-zone heating climate entities, target setpoint overrides, and mode synchronizations.
* **`test_devices_fan.cpp`**:  
  Unit tests for ventilation / fan platform across OEM schemes (Orcon, Vasco, Itho, Zehnder).
* **`test_devices_water_heater.cpp`**:  
  Unit tests for Domestic Hot Water (DHW) cylinder temperatures, setpoints, and operating states.

---

## 3. How to Run the Tests

### C++ Native Unit Tests & Corpus Regression (Instant Native Execution)
```bash
mkdir -p tests/build && cd tests/build
cmake ..
make -j4
ctest --output-on-failure
```

### Full Python & C++ Parity Harness
```bash
uv run python tests/test_parity_harness.py
```

The Python half requires the `ramses_rf`/`ramses_tx` dependencies to be installed in
the active environment. The native C++ parity target can be run independently with
`ctest --test-dir tests/build -R ParityJsonTest`.

### C. Adding a New Test Case
To add coverage for a new opcode or vendor quirk:
1. Open `tests/fixtures/parity_cases.json`.
2. Add an entry with the test case name, description, raw HGI80 line, and expected decoded fields.
3. Run `python3 tests/test_parity_harness.py` to verify that both `ramses_rf` and the C++ decoders pass.
