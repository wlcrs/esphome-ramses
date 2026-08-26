#!/usr/bin/env python3
"""Validate the deterministic parity subset with ramses_rf."""

from __future__ import annotations

from collections import Counter
from datetime import datetime as dt
from datetime import timezone
from pathlib import Path

from extract_parity_corpus import SUPPORTED_OPCODES, extract, normalise_frame
from ramses_rf.messages import Message
from ramses_tx.packet import Packet


def main() -> int:
    source = (
        Path(__file__).resolve().parents[1]
        / "ramses_rf/tests/fixtures/regression_packets_sorted.txt"
    )
    cases = extract(source)
    counts: Counter[str] = Counter()
    valid_frames = 0
    for line in source.read_text(encoding="utf-8", errors="replace").splitlines():
        result = normalise_frame(line)
        if result is None:
            continue
        frame, opcode, _ = result
        packet = Packet.from_port(dt.now(timezone.utc), frame)
        message = Message(packet.to_dto() if hasattr(packet, "to_dto") else packet)
        assert f"{int(message.code, 16):04X}" == opcode
        assert str(message.verb).strip() in {"I", "RP"}
        counts[opcode] += 1
        valid_frames += 1
    assert valid_frames >= 10000
    assert set(counts) == SUPPORTED_OPCODES
    print(
        f"Validated {valid_frames} full-corpus frames and {len(cases)} variant cases across {len(counts)} opcodes."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
