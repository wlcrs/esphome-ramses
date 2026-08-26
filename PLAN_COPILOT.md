# Plan: Close the Static-Configuration Compatibility Gaps

## Scope and Architectural Boundary

`ramses_devices` is a statically configured ESPHome entity layer. Device discovery,
commissioning, device classification, and YAML generation are deliberately owned by
the separate external discovery component. A user runs that tool first, receives the
appropriate YAML fragments, and then flashes an ESPHome firmware with explicit entity
configuration.

This plan therefore does **not** propose dynamic discovery, runtime device registries,
automatic device promotion, or a general-purpose device model hierarchy in
`ramses_devices`.

The target is not feature parity with every one of the 107 payload opcodes registered
by `ramses_rf`. The target is dependable static ESPHome support for the device/entity
features that the discovery tool can generate, with protocol behavior verified against
`ramses_rf` and relevant `ramses_cc` behavior.

## Current Baseline

- Native C++ tests currently pass all 10 CMake targets.
- The implemented ESPHome platforms are `sensor`, `binary_sensor`, `climate`, `fan`,
  and `water_heater`.
- Current protocol support covers the main heating, HVAC, OpenTherm, DHW, battery,
  contact, filter, and binding payloads.
- `ramses_rf` provides the canonical reference implementation and has 107 registered
  four-digit payload opcodes in this checkout.
- `ramses_rf` has a 32,325-line regression packet fixture.
- The local C++ corpus contains 170 `packet.log` files, but the corpus test currently
  validates aggregate counts rather than payload-by-payload semantic parity.

## Phase 1: Make the Existing Test Claims Trustworthy

### 1.1 Correct corpus opcode routing

Fix the dispatch in `tests/test_parser_corpus.cpp`:

- Route `0x0005` to `ZoneStructurePayload`.
- Route `0x000C` to `ZoneRolePayload`.
- Remove or investigate the `0x000A` route unless the protocol reference confirms
  that this project intentionally supports it.
- Ensure `0x22E5` is tested if the decoder claims support for it.

### 1.2 Strengthen corpus assertions

The corpus test should report and assert:

- Number of files discovered and processed.
- Number of non-empty candidate frames.
- Number of invalid HGI80 frames.
- Counts per opcode.
- Decode failures per known opcode.
- At least one successfully decoded payload for every supported decoder.

Malformed or unsupported packets may be counted and reported, but must not silently
disappear from the test result.

### 1.3 Replace fragile fixture parsing

Replace the hand-written JSON substring extraction in `tests/test_parity_cases.cpp`
with a structured parser already available to the test toolchain, or constrain the
fixture format and validate it explicitly. Nested objects, escaped strings, field
ordering, and missing expected fields must not change test meaning.

## Phase 2: Establish Differential Payload Parity

### 2.1 Define a common expected-value representation

Extend `tests/fixtures/parity_cases.json` so each case can describe decoded semantic
fields, including:

- Header and address fields.
- Scalar values and units.
- Optional values.
- Invalid/sentinel values.
- Multi-zone or multi-item payloads.
- Vendor-specific variants.

### 2.2 Make both sides compare semantics

Update `tests/test_parity_harness.py` and the C++ runner so they compare expected
payload fields rather than only packet headers or selected raw byte offsets. The
Python side should use `ramses_rf` as the reference result; the C++ side should expose
the same normalized fields.

### 2.3 Reuse the Python regression corpus selectively

Do not copy the complete 32,325-line fixture into every native test invocation.
Instead, add a deterministic extraction step that produces a checked-in or generated
subset containing:

- Every currently supported opcode.
- Every supported payload length or variant.
- Sentinel and negative-temperature cases.
- Representative heat, DHW, HVAC, OpenTherm, TRV, actuator, and controller traffic.
- Packets from the vendor schemes supported by `RamsesFan`.

The extraction should preserve the original packet and identify its source opcode.

## Phase 3: Complete and Harden Existing ESPHome Platforms

### 3.1 Climate

- Verify the mapping of `0x1F09` system modes to ESPHome climate modes and presets.
- Test simultaneous target-temperature, mode, and preset calls.
- Confirm that commands are sent only when the relevant call field is present.
- Test invalid and unavailable zone values.
- Verify that state is confirmed by inbound packets rather than relying only on
  optimistic local state where practical.
- Document the intentional limitation that one climate entity represents one
  statically configured zone.

### 3.2 Fan

- Build a packet matrix for Orcon, Vasco, Itho, and Zehnder.
- Verify `0x22F1`, `0x22F3`, and any supported vendor-specific variants against
  `ramses_rf`.
- Test speed percentage, preset transitions, OFF behavior, and unknown modes.
- Test pairing timeout, offer/confirm handling, persistence, and YAML-vs-persisted
  address precedence.

### 3.3 Water heater

- Verify the exact semantics and variants of `0x1260`, `0x12F0`, and `0x1F41`.
- Implement actual mode commands for OFF, ECO, PERFORMANCE, and ELECTRIC where the
  RAMSES protocol and target device support them; do not only update `mode_` locally.
- Define how target temperature writes are confirmed or rejected.
- Test unavailable/sentinel temperatures and disabled DHW state.
- Verify traits against the device families for which generated YAML is intended.

### 3.4 Sensors and binary sensors

- Verify address filtering for source, destination, and broadcast messages.
- Add tests for every configured sensor and binary sensor type.
- Test multi-zone selection and nonmatching zone entries.
- Define sentinel behavior consistently: retain previous state, publish unavailable,
  or suppress publication, according to the corresponding `ramses_rf` semantics.
- Add missing diagnostic mappings only when they correspond to a concrete static YAML
  use case.

## Phase 4: Add High-Value Static Entity Features

Prioritize features that can be configured directly from discovery-generated YAML and
that map cleanly to existing ESPHome platforms.

### 4.1 Relay and actuator telemetry

Expose `0x0008` relay demand as a documented static sensor configuration with optional
relay-index filtering. Control is a separate task and must not be implied by telemetry
support. The implementation should remain explicit YAML rather than introducing
runtime actuator discovery.

**Status:** Complete for relay-demand telemetry. Relay control remains out of scope for
this telemetry phase.

### 4.2 Additional OpenTherm diagnostics

Add static sensor/binary-sensor options for fields already decoded or directly
supported by `ramses_rf`, such as fault state, flame state, modulation, flow, and
return temperature. Add fault-code support only with a defined source opcode and
fixture coverage.

### 4.3 HVAC diagnostics

Expose bypass position/activity, filter state, filter lifetime, and supply/outdoor
air values where the payload decoder can distinguish them reliably. Relay demand,
bypass position/activity, and filter lifetime/remaining-percent are now available as
explicit static YAML types. Keep future additions explicit rather than introducing
runtime device classes.

### 4.4 Optional number/select/button platforms

Evaluate these only for concrete commands needed by generated static YAML. Likely
candidates are:

- A number entity for DHW target temperature.
- A select entity for system/DHW/fan modes.
- Buttons for filter reset or explicit query/pairing actions.

Each candidate requires a protocol command builder, an inbound confirmation path, and
an ESPHome-specific test. Do not add generic entities merely to match the platform
count in `ramses_cc`.

**Status:** Existing modulation, flow/return temperature, flame, and fault entities are
covered. Fault-code payloads and additional command-oriented entities remain future
work until a concrete static YAML use case is identified.

## Phase 5: Protocol Expansion by Use Case

After the existing platforms and parity harness are reliable, add missing opcodes in
priority order based on actual discovery output and user configurations:

1. Payload variants required by currently supported heating, DHW, HVAC, and
   OpenTherm devices.
2. Fault and diagnostic payloads needed for useful static diagnostics.
3. Schedule/configuration payloads only if the ESPHome component will expose a clear
   static or command-oriented API for them.
4. Additional binding and commissioning payloads only where they support the existing
   fan pairing workflow.

Each opcode addition must include:

- A C++ decoder or encoder with a `ramses_rf` reference.
- Valid-length, truncated, sentinel, and variant tests.
- At least one real packet from the local corpus.
- Differential comparison against `ramses_rf`.
- An explicit statement of whether it is telemetry-only, query-capable, or writable.

## Phase 6: ESPHome Configuration and Compatibility Validation

- Compile representative static YAML generated by the external discovery component.
- Add examples covering one heating system, one MVHR system, one DHW system, and one
  OpenTherm system.
- Validate multiple entities sharing the same `ramses_esp` parent.
- Validate multiple zones and multiple sensors for one controller.
- Check that all generated configuration keys remain stable and documented.
- Keep discovery-component tests separate; here, only consume its generated YAML
  contract.

## Reusable Tests from `ramses_rf` and `ramses_cc`

### Directly reusable as reference or fixture sources

- `ramses_rf/tests/tests_rf/test_payload_codecs.py`: codec and field expectations.
- `ramses_rf/tests/tests_rf/test_payload_regression_shadow.py`: broad real-packet
  regression strategy.
- `ramses_rf/tests/tests_rf/test_rx_payload_decoder.py`: packet decoding behavior.
- `ramses_rf/tests/tests_rf/test_payload_structure.py`: payload length/structure
  validation.
- `ramses_rf/tests/tests_rf/device/test_hvac_ventilator.py`: vendor fan behavior.
- `ramses_rf/tests/tests_rf/data_driven/test_devices.py`: device/message fixtures.

### Behavioral tests to port conceptually

- `ramses_cc/tests/tests_new/test_climate.py`
- `ramses_cc/tests/tests_new/test_sensor.py`
- `ramses_cc/tests/tests_new/test_binary_sensor.py`
- `ramses_cc/tests/tests_new/test_number.py`
- `ramses_cc/tests/tests_new/test_water_heater.py`
- `ramses_cc/tests/tests_new/test_fan_handler.py`
- `ramses_cc/tests/tests_new/test_discovery_manager.py` only for understanding the
  generated YAML contract, not for moving discovery logic into `ramses_devices`.

Python/Home Assistant tests should generally not be copied verbatim into C++. Port
their input packets, expected state transitions, command bytes, and edge cases into
focused native tests.

## Definition of Done

- Every supported C++ decoder has representative real-packet coverage.
- Differential tests compare semantic fields against `ramses_rf`.
- The corpus test cannot pass while silently skipping all known decoder failures.
- Climate, fan, and water-heater commands have packet-level tests and inbound state
  confirmation tests.
- Static YAML generated by the external discovery component compiles for all supported
  entity families.
- Documentation clearly lists supported entity types, opcodes, device assumptions,
  and unsupported features.
- No dynamic discovery or runtime device-model framework is introduced into
  `ramses_devices`.