# Implementation Plan: Native RAMSES II Support in ESPHome

This document provides a comprehensive, step-by-step roadmap for implementing native RAMSES II decoding, dedicated Home Assistant platforms under `ramses_devices` (`sensor`, `binary_sensor`, `climate`, `fan`, `water_heater`), and finally the dedicated auto-discovery tool (`ramses_discovery`) in ESPHome.

---

## 1. Architectural Principles & Component Separation

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ LAYER 3: DISCOVERY & COMMISSIONING (`ramses_discovery`)                           │
│ Standalone interactive sniffer, active RQ 0004 zone namer, and YAML generator    │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ LAYER 2: PLATFORMS & ENTITIES (`components/ramses_devices/`)                     │
│ High-level HA entities: climate, fan, sensor, binary_sensor, water_heater        │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │ (Subscribes to decoded messages / transmits commands)
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ LAYER 1: PURE PHYSICAL / MAC TRANSCEIVER HUB (`components/ramses_esp/`)          │
│ CC1101 SPI driver, Manchester codec, HGI80 framing, queues, USB/WiFi streaming   │
└──────────────────────────────────────────────────────────────────────────────────┘
```

1. **Pure `ramses_esp` Transceiver Hub**:
   * The `components/ramses_esp/` component remains 100% pure as the low-level physical/MAC layer (CC1101 SPI driver, Manchester encoding/decoding, HGI80 framing, packet queues, and raw packet callbacks).
   * No Home Assistant platform entities are defined in `ramses_esp`.
2. **Dedicated Platform Component (`components/ramses_devices/`)**:
   * All Home Assistant entities (`sensor.ramses_devices`, `binary_sensor.ramses_devices`, `climate.ramses_devices`, `fan.ramses_devices`, `water_heater.ramses_devices`) live cleanly inside `components/ramses_devices/`.
   * Each entity references the parent `ramses_esp` hub via `ramses_esp_id: <hub_id>`.
3. **Modular & Separate Discovery Component (`components/ramses_discovery/`)**:
   * Discovery is implemented **last** after all platform entities in `ramses_devices` are complete, so it can generate full, ready-to-use YAML snippets for the user.
4. **Traceability to `ramses_rf`**:
   * Every C++ parser function and struct MUST include explicit header comments referencing the corresponding Python module and class/function in `ramses_rf`.
5. **Universal Mock Subsystem (`mock_ramses_esp`)**:
   * A configurable simulation engine that produces real HGI80 streams to test **both** our C++ ESPHome components and Python `ramses_rf` / `ramses_cc` for 100% processing parity.

---

## 2. Multi-Phase Implementation Roadmap

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│ PHASE 0: Testing Foundation & Universal Mock Subsystem (`mock_ramses_esp`) [DONE] │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 1: Zero-Heap C++ Opcode Codecs (`ramses_decoder`) + Parity Harness   [DONE] │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 2: Telemetry Sensor & Binary Sensor Platforms (`sensor/binary_sensor.ramses_devices`) │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 3: Climate Platform (`climate.ramses_devices` for Evohome / Heating)       │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 4: Ventilation / Fan Platform (`fan.ramses_devices` for MVHR Units)        │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 5: Domestic Hot Water (`water_heater.ramses_devices`) & OpenTherm Diagnostics │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 6: Dedicated Discovery Component (`ramses_discovery`) & YAML Generator     │
└────────────────────────────────────────┬─────────────────────────────────────────┘
                                         │
┌────────────────────────────────────────▼─────────────────────────────────────────┐
│ PHASE 7: End-to-End Dual Parity Test Suite, CI Integration & Final Polish         │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

### Phase 0: Testing Foundation & Universal Mock Subsystem [COMPLETED]

**Objective**: Build the configurable `mock_ramses_esp` simulator first so every subsequent phase has instant automated testing and differential validation against `ramses_rf`.

#### Delivered:
* `tests/mock/mock_ramses_esp.h` / `mock_ramses_esp.cpp`: Configurable virtual device simulators (`MockEvohomeController`, `MockMvhrVentilator`, `MockTrv`, `MockOpenThermBridge`).
* `tests/fixtures/parity_cases.json`: Canonical declarative test cases defining raw HGI80 packet inputs and expected decoded fields.
* `tests/test_parity_cases.cpp`: C++ test runner loading `parity_cases.json` and asserting decoding.
* `tests/test_parity_harness.py`: Python parity test runner validating `parity_cases.json` against `ramses_rf` in `.venv`.
* `tests/README.md`: Architectural overview and testing guide.

---

### Phase 1: Zero-Heap C++ Opcode Codecs (`ramses_decoder`) [COMPLETED]

**Objective**: Implement strongly typed, zero-heap C++ payload decoders and command builders with explicit references to `ramses_rf`.

#### Delivered:
* `components/ramses_esp/ramses_decoder.h` & `ramses_decoder.cpp`:
  * **Temperature Payloads (`0x30C9`, `0x12C0`)**: Multi-zone array unpacking, fixed-point math (`0.01°C`), sentinel checks (`0x7FFF`, `0x7EFF`), negative temperature handling.  
    *Reference: `ramses_rf/payloads/heating.py:TemperaturePayload` / `OutdoorTempPayload`*
  * **Setpoint Payloads (`0x2309`)**: Setpoint array unpacking & write command builder (`W 2309`).  
    *Reference: `ramses_rf/payloads/heating.py:SetpointPayload`*
  * **System Mode & Sync (`0x1F09`)**: Operating modes (`Auto`, `Away`, `Day Off`, `Custom`, `Eco`, `Off`) & write builder (`W 1F09`).  
    *Reference: `ramses_rf/payloads/system.py:SystemSyncPayload`*
  * **Heat & Relay Demand (`0x3150`, `0x0008`)**: Heat demand % & wireless relay actuator state.  
    *Reference: `ramses_rf/payloads/heating.py:HeatDemandPayload` & `RelayDemandPayload`*
  * **Zone Structure (`0x0005`), Bindings (`0x000C`) & Names (`0x0004`)**: Active zone bitmasks, device bindings, and ASCII room name decoding/querying.  
    *Reference: `ramses_rf/payloads/heating.py:ZoneNamePayload` / `ZoneStructurePayload`*
  * **HVAC / MVHR Control (`0x22F1`, `0x10A0`, `0x10D0`, `0x12A0`, `0x1298`, `0x10E0`)**: Fan modes (Orcon, Vasco, Itho, Zehnder), bypass damper, filter days, CO2 ppm, multi-sensor air quality, OEM signatures.  
    *Reference: `ramses_rf/payloads/hvac.py`*
  * **OpenTherm (`0x3220`) & DHW (`0x1260`, `0x12F0`, `0x1F41`)**: Modulation %, flame status, flow/return temps, DHW temp/setpoint/relay state.  
    *Reference: `ramses_rf/payloads/opentherm.py` & `ramses_rf/payloads/dhw.py`*
  * **Device Battery & State (`0x1060`, `0x12B0`)**: Battery percentage, low battery alarms, window contact state.  
    *Reference: `ramses_rf/payloads/helpers.py` & `ramses_rf/payloads/heating.py`*
* `tests/test_opcode_codecs.cpp`: Unit tests covering standard decoders, edge cases (sub-zero temps, sentinels, truncated payloads, malformed inputs).
* **Validation**: 100% passing in `ctest` (4/4 test suites) and Python parity harness (17/17 cases).

---

### Phase 2: Telemetry Sensor & Binary Sensor Platforms (`components/ramses_devices/`)

**Objective**: Expose temperatures, setpoints, heat demands, air quality, battery levels, and diagnostic binary states as native ESPHome sensors under `platform: ramses_devices`.

#### Steps:
1. Implement `components/ramses_devices/__init__.py`:
   * Main component registering `ramses_devices` domain and linking to `ramses_esp_id`.
2. Implement `components/ramses_devices/sensor/`:
   * `temperature` (Zone temp, outdoor temp via `0x30C9` / `0x12C0`).
   * `setpoint` (Current target setpoint via `0x2309`).
   * `heat_demand` (0.0% to 100.0% via `0x3150`).
   * `co2` (ppm via `0x1298`).
   * `indoor_humidity`, `outdoor_humidity` (via `0x12A0`).
   * `filter_remaining_days` (via `0x10D0`).
   * `opentherm_modulation`, `return_temperature`, `flow_temperature` (via `0x3220`).
   * `battery_level` (via `0x1060`).
3. Implement `components/ramses_devices/binary_sensor/`:
   * `filter_alarm` (via `0x10A0` / `0x10D0`).
   * `flame_active` / `fault_alarm` (via `0x3220` / `0x0418`).
   * `window_open` (via `0x12B0`).
   * `battery_low` (via `0x1060`).

#### Validation & Testing:
* Automated unit tests feeding `MockRamsesEsp` telemetry and verifying ESPHome sensor `publish_state()` values.
* ESPHome compilation of example sensor YAML configuration (`example-c6.yaml`).

---

### Phase 3: Climate Platform (`climate.ramses_devices` for Evohome / CH)

**Objective**: Implement native ESPHome `climate` entities under `platform: ramses_devices` for Evohome zones and heating controllers with bidirectional setpoint and system mode control.

#### Steps:
1. Implement `components/ramses_devices/climate/`:
   * Class `RamsesZoneClimate : public climate::Climate, public Component`.
   * YAML config schema: `ramses_esp_id`, `controller_id`, `zone_index`.
   * Visual traits: Current temperature, Target setpoint, Preset modes (`auto`, `away`, `eco`, `day_off`, `custom`, `off`).
2. Implement Bidirectional Control:
   * Handle `control(const ClimateCall &call)`:
     * Target temp change: Transmit `W 2309` with zone index and target setpoint.
     * Preset mode change: Transmit `W 1F09` with new system mode.
3. Handle Inbound Updates:
   * Route incoming `0x30C9` (current temp), `0x2309` (target setpoint), and `0x1F09` (system mode) to update climate state.

#### Validation & Testing:
* Integration test: Change climate temperature in test harness -> assert `W 2309` packet constructed -> mock controller echoes `I 2309` -> climate state confirms target temp.

---

### Phase 4: Ventilation / Fan Platform (`fan.ramses_devices` for MVHR Units)

**Objective**: Implement native ESPHome `fan` entities under `platform: ramses_devices` for MVHR units supporting multi-vendor speed and mode schemes (Orcon, Vasco, Itho, Zehnder).

#### Steps:
1. Implement `components/ramses_devices/fan/`:
   * Class `RamsesFan : public fan::Fan, public Component`.
   * YAML schema: `ramses_esp_id`, `device_id`, `scheme` (`auto`, `orcon`, `itho`, `vasco`, `zehnder`).
   * Supported presets: `auto`, `low`, `medium`, `high`, `boost`, `away`.
2. Implement Fan Control:
   * Speed percentage (0-100%) and preset mode selection.
   * Command builder: Transmit `W 22F1` / `W 22F3` formatted for the configured vendor scheme.
3. Telemetry Ingestion:
   * Ingest `0x22F1` (current mode/speed), `0x10A0` (bypass state, fan status), `0x10D0` (filter days).

#### Validation & Testing:
* Unit tests for each vendor scheme (`orcon`, `itho`, `vasco`, `zehnder`) verifying speed byte encoding and state decoding.

---

### Phase 5: Domestic Hot Water (`water_heater.ramses_devices`) & OpenTherm Diagnostics

**Objective**: Implement DHW cylinder support and advanced OpenTherm telemetry under `platform: ramses_devices`.

#### Steps:
1. Implement `components/ramses_devices/water_heater/`:
   * Expose Evohome DHW as a standard ESPHome `water_heater` entity.
   * Support `0x1260` (current DHW temp & target setpoint), `0x12F0` (DHW config), and `0x1F41` (DHW relay state & mode: On / Off / Auto).
   * Transmit `W 1260` when target water temperature is adjusted.
2. Advanced OpenTherm entity exposure (`0x3220`):
   * Boiler modulation %, relative flame state, return water temperature, boiler fault codes.

#### Validation & Testing:
* Unit tests verifying DHW setpoint writes (`W 1260`) and relay status parsing.

---

### Phase 6: Dedicated Auto-Discovery Component (`ramses_discovery`)

**Objective**: Provide an interactive discovery tool that passively sniffs the network, interrogates Evohome controllers for zone names (`RQ 0004`), queries MVHR OEM signatures (`RQ 10E0`), and outputs full, ready-to-use YAML configuration blocks for `ramses_devices` platforms (`sensor`, `climate`, `fan`, `water_heater`).

#### Steps:
1. Create `components/ramses_discovery/__init__.py`, `ramses_discovery.h`, `ramses_discovery.cpp`.
2. Implement **Passive Device Tracking**:
   * Device address table indexing seen devices (`01:`, `04:`, `10:`, `13:`, `32:`, `34:`).
   * OEM signature detection for MVHR units (`0x67` -> Orcon, `0x08` -> Itho, `0x13` -> Vasco, `0x02` -> Zehnder).
   * Real-time logging of user physical button presses (TRV adjustments, remote clicks).
3. Implement **Active Controller Interrogation**:
   * When an Evohome controller (`01:xxxxxx`) is detected, send throttled `RQ 0005` (zone structure) and `RQ 0004` (room names) to retrieve zone labels.
4. Output YAML generator:
   * Print formatted YAML blocks directly to ESPHome serial/web logs.
   * Expose diagnostic text sensor `discovered_yaml`.
   * Generates complete YAML targeting `platform: ramses_devices`.

#### Validation & Testing:
* Host integration test with `MockRamsesEsp`: Inject simulated Evohome + MVHR packets, verify discovery component logs the exact room names, vendor schemes, and valid entity YAML blocks.
* Compile and test `example-c6-discovery.yaml` with ESPHome.

---

### Phase 7: End-to-End Dual Parity Test Suite, CI Integration & Final Polish

**Objective**: Run continuous differential testing against `ramses_rf` / `ramses_cc` in GitHub Actions CI across all platforms.

#### Steps:
1. Update `.github/workflows/ci.yml`:
   * Add CMake C++ test build & execution (`ctest --output-on-failure`).
   * Add Python dual-sided parity validation (`python tests/test_parity_harness.py`).
   * Add ESPHome firmware compilation validation for sample YAML configurations (`example-c6.yaml`, `example-c6-discovery.yaml`).
2. Update documentation and user guide for setting up RAMSES with ESPHome.

---

## 3. Summary of Deliverables

| Deliverable | Path | Description |
| :--- | :--- | :--- |
| **Physical Hub** | `components/ramses_esp/` | Pure CC1101 SPI driver, Manchester codec, HGI80 packet router |
| **Opcode Codecs** | `components/ramses_esp/ramses_decoder.*` | Zero-heap C++ parsers/encoders with `ramses_rf` code references [DONE] |
| **Entity Platforms** | `components/ramses_devices/` | `climate`, `fan`, `sensor`, `binary_sensor`, `water_heater` platforms |
| **Discovery Component** | `components/ramses_discovery/` | Dedicated passive sniffer, active `RQ 0004` zone namer, and YAML generator |
| **Mock Simulator** | `tests/mock/mock_ramses_esp.*` | Configurable simulation engine (Evohome, MVHR, TRVs, OpenTherm) [DONE] |
| **Parity Harness** | `tests/test_parity_harness.py` | Differential parity tester comparing C++ and `ramses_rf` outputs [DONE] |
| **CI Workflow** | `.github/workflows/ci.yml` | Automated unit test, parity test, and ESPHome compilation pipeline |
