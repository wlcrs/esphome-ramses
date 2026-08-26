#!/usr/bin/env python3
"""Run native C++ semantic parity over the supported corpus subset."""

from __future__ import annotations

import json
import subprocess
import tempfile
from datetime import datetime as dt
from datetime import timezone
from pathlib import Path

from extract_parity_corpus import extract
from ramses_rf.messages import Message
from ramses_tx.packet import Packet


def make_case(case: dict[str, object]) -> dict[str, object] | None:
    packet = Packet.from_port(dt.now(timezone.utc), case["hgi80"])
    message = Message(packet.to_dto() if hasattr(packet, "to_dto") else packet)
    payload = getattr(message, "payload", getattr(message, "data", {}))
    opcode = case["opcode"]
    if int(str(message.src.id).split(":", 1)[0]) > 37:
        return None
    expected: dict[str, object] = {
        "verb": str(message.verb).strip(),
        "src": str(message.src.id),
        "dst": "--:------",
        "opcode": opcode,
    }

    if opcode == "0004" and isinstance(payload, dict) and "name" in payload:
        expected.update(
            zone_index=int(payload["zone_index"], 16), zone_name=payload["name"]
        )
    elif opcode == "0008" and isinstance(payload, dict) and "relay_demand" in payload:
        expected.update(demand_pct=payload["relay_demand"] * 100)
    elif opcode == "10D0" and isinstance(payload, dict) and "days_remaining" in payload:
        expected["filter_remaining_days"] = payload["days_remaining"]
    elif (
        (
            opcode == "1260"
            and isinstance(payload, dict)
            and payload.get("temperature") is not None
        )
        or opcode == "12C0"
        and isinstance(payload, dict)
        and "temperature" in payload
    ):
        expected["temperature"] = payload["temperature"]
    elif (
        opcode == "12F0"
        and isinstance(payload, dict)
        and payload.get("dhw_flow_rate") is not None
    ):
        expected["flow_rate"] = payload["dhw_flow_rate"]
    elif opcode == "1298" and isinstance(payload, dict) and "co2_level" in payload:
        expected["co2_ppm"] = payload["co2_level"]
    elif opcode == "3150" and isinstance(payload, dict) and "heat_demand" in payload:
        expected.update(
            domain_or_zone_index=int(payload["zone_index"], 16),
            demand_pct=payload["heat_demand"] * 100,
        )
    elif opcode == "2E04" and isinstance(payload, dict) and "system_mode" in payload:
        raw_payload = bytes.fromhex(case["hgi80"].split()[-1])
        expected.update(
            system_mode_raw=raw_payload[0], system_mode=payload["system_mode"]
        )
    elif opcode == "30C9":
        items = payload if isinstance(payload, list) else [payload]
        if not all(
            isinstance(item, dict) and item.get("temperature") is not None
            for item in items
        ):
            return None
        expected["temperatures"] = [
            {"temperature": item["temperature"]} for item in items
        ]
    else:
        return None

    return {
        "name": f"corpus_{opcode}_{case['length']}",
        "hgi80": case["hgi80"],
        "expected": expected,
    }


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    source = root / "ramses_rf/tests/fixtures/regression_packets_sorted.txt"
    cases = []
    for case in extract(source):
        native_case = make_case(case)
        if native_case is not None:
            cases.append(native_case)
    assert len(cases) >= 12
    with tempfile.NamedTemporaryFile(
        "w", suffix=".json", encoding="utf-8", delete=False
    ) as fixture:
        json.dump(cases, fixture)
        fixture_path = fixture.name
    try:
        result = subprocess.run(
            [str(root / "tests/build/test_parity_cases"), fixture_path],
            check=False,
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
    finally:
        Path(fixture_path).unlink(missing_ok=True)
    print(result.stdout)
    if result.returncode != 0:
        return result.returncode
    print(f"Validated {len(cases)} native semantic corpus cases.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
