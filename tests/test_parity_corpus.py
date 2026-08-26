#!/usr/bin/env python3
"""Validate the deterministic parity subset with ramses_rf."""

from __future__ import annotations

from datetime import datetime as dt
from pathlib import Path

from ramses_rf.messages import Message
from ramses_tx.packet import Packet

from extract_parity_corpus import extract


def main() -> int:
    source = Path(__file__).resolve().parents[1] / "ramses_rf/tests/fixtures/regression_packets_sorted.txt"
    cases = extract(source)
    for case in cases:
        packet = Packet.from_port(dt.now(), case["hgi80"])
        message = Message(packet.to_dto() if hasattr(packet, "to_dto") else packet)
        assert f"{int(message.code, 16):04X}" == case["opcode"]
        assert str(message.verb).strip() in {"I", "RP"}
    print(f"Validated {len(cases)} corpus parity cases across 23 opcodes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())