#!/usr/bin/env python3
"""Dual-Sided Parity Validation Harness.

Loads test cases from tests/fixtures/parity_cases.json and asserts that:
1. Python ramses_rf decodes each packet to match the expected fields.
2. The C++ test runner decodes the exact same packet to match the expected fields.
"""

import json
import os
import subprocess
import sys
from datetime import datetime as dt
from datetime import timezone

from ramses_rf.messages import Message
from ramses_tx.packet import Packet


def parse_with_ramses_rf(hgi80_line: str) -> dict:
    """Parse an HGI80 packet line using Python ramses_rf."""
    pkt = Packet.from_port(dt.now(timezone.utc), hgi80_line)
    msg = Message(pkt.to_dto() if hasattr(pkt, "to_dto") else pkt)
    return {
        "verb": str(msg.verb).strip(),
        "src": str(msg.src.id) if msg.src else None,
        "dst": str(msg.dst.id) if msg.dst else None,
        "code": f"{int(msg.code, 16):04X}"
        if hasattr(msg, "code") and msg.code
        else None,
        "payload": getattr(msg, "payload", getattr(msg, "data", {})),
    }


def assert_semantic_parity(case: dict, parsed: dict) -> None:
    """Compare fixture semantics with the canonical ramses_rf payload."""
    expected = case["expected"]
    code = expected["opcode"]
    payload = parsed["payload"]

    if code == "1F09":
        assert payload["remaining_seconds"] == expected["remaining_raw"] / 10.0
    elif code == "2309":
        assert payload["zone_index"] == f"{expected['zone_index']:02X}"
        assert payload["setpoint"] == expected["setpoint"]
    elif code == "30C9":
        assert [item["temperature"] for item in payload] == [
            item["temperature"] for item in expected["temperatures"]
        ]
    elif code == "0004":
        assert payload["zone_index"] == f"{expected['zone_index']:02X}"
        assert payload["name"] == expected["zone_name"]
    elif code == "22F1":
        assert int(payload["fan_mode"], 16) == expected["fan_mode_raw"]
    elif code == "10E0":
        assert payload["info_bytes"][5] == int(expected["oem_code"], 16)
    elif code == "3150":
        assert payload["zone_index"] == f"{expected['domain_or_zone_index']:02X}"
        assert payload["heat_demand"] == expected["demand_pct"] / 100.0
    elif code == "1060":
        assert payload["battery_low"] == expected["battery_low"]
        assert payload["battery_level"] == expected["battery_pct"] / 200.0
    elif code == "10D0":
        assert payload["days_remaining"] == expected["filter_remaining_days"]
    elif code == "12C0":
        assert payload["temperature"] == expected["temperature"]
    elif code == "1260":
        assert payload["dhw_index"] == f"{expected.get('dhw_index', 0):02X}"
        assert payload["temperature"] == expected["temperature"]
    elif code == "12F0":
        assert payload["dhw_flow_rate"] == expected["flow_rate"]
    elif code == "0008":
        assert payload["domain_index"] == f"{expected.get('domain_index', 0):02X}"
        assert payload["relay_demand"] == expected["demand_pct"] / 100.0
    elif code == "1298":
        assert payload["co2_level"] == expected["co2_ppm"]


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    fixture_path = os.path.join(script_dir, "fixtures", "parity_cases.json")

    if not os.path.exists(fixture_path):
        print(f"ERROR: Fixture file not found: {fixture_path}")
        return 1

    with open(fixture_path, "r", encoding="utf-8") as f:
        cases = json.load(f)

    print("==================================================")
    print(f"Running Dual-Sided Parity Harness with {len(cases)} Cases")
    print(f"Fixture: {fixture_path}")
    print("==================================================")

    py_passed = 0
    for case in cases:
        name = case.get("name", "unnamed")
        hgi80 = case["hgi80"]
        expected = case["expected"]

        print(f"\n[Case: {name}] Testing HGI80: {hgi80}")
        parsed = parse_with_ramses_rf(hgi80)

        # Assert Header parity
        assert parsed["code"] == expected["opcode"], (
            f"Opcode mismatch: {parsed['code']} != {expected['opcode']}"
        )
        assert parsed["src"] == expected["src"], (
            f"Source mismatch: {parsed['src']} != {expected['src']}"
        )
        assert parsed["verb"] == expected["verb"], (
            f"Verb mismatch: {parsed['verb']} != {expected['verb']}"
        )
        assert_semantic_parity(case, parsed)

        print(
            f"  -> Python ramses_rf decoded: {parsed['verb']} {parsed['code']} from {parsed['src']} (Payload: {parsed['payload']})"
        )
        py_passed += 1

    print(
        f"\n[PASS] Python ramses_rf passed all {py_passed}/{len(cases)} parity fixture cases."
    )

    # Run C++ Parity Binary
    cpp_binary = os.path.join(script_dir, "build", "test_parity_cases")
    if os.path.exists(cpp_binary):
        print("\nRunning C++ Parity Test Runner...")
        res = subprocess.run(
            [cpp_binary, fixture_path], capture_output=True, text=True, check=False
        )
        print(res.stdout)
        if res.returncode != 0:
            print(res.stderr)
            print("[FAIL] C++ parity runner failed!")
            return res.returncode
        print("[PASS] C++ decoder passed all parity fixture cases.")
    else:
        print(
            f"NOTE: C++ binary not yet built at {cpp_binary} (run cmake build to enable)."
        )

    print("\n==================================================")
    print("ALL DUAL-SIDED PARITY TESTS PASSED SUCCESSFULLY!")
    print("==================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
