#!/usr/bin/env python3
"""Extract a deterministic inbound parity subset from the ramses_rf corpus."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

SUPPORTED_OPCODES = {
    "0004",
    "0005",
    "0008",
    "000C",
    "1060",
    "10A0",
    "10D0",
    "10E0",
    "1260",
    "1298",
    "12A0",
    "12B0",
    "12C0",
    "12F0",
    "1F09",
    "1F41",
    "22E5",
    "22F1",
    "22F3",
    "2309",
    "2E04",
    "30C9",
    "3150",
    "3220",
}
VERBS = {"I", "RP"}
HEX_RE = re.compile(r"^[0-9A-Fa-f]+$")


def normalise_frame(line: str) -> tuple[str, str, int] | None:
    line = line.split("#", 1)[0].strip()
    if not line:
        return None
    tokens = line.split()
    for index, token in enumerate(tokens):
        if token not in VERBS or index == 0:
            continue
        if index + 4 >= len(tokens) or not tokens[index - 1].isdigit():
            continue
        opcode_index = index + 5
        length_index = opcode_index + 1
        payload_index = opcode_index + 2
        opcode = tokens[opcode_index].upper()
        if (
            opcode not in SUPPORTED_OPCODES
            or length_index >= len(tokens)
            or payload_index >= len(tokens)
            or not tokens[length_index].isdigit()
        ):
            continue
        length = int(tokens[length_index])
        payload = tokens[payload_index]
        if (
            len(opcode) != 4
            or len(payload) != length * 2
            or not HEX_RE.fullmatch(payload)
        ):
            continue
        separator = "  " if token == "I" else " "
        frame = f"{tokens[index - 1]}{separator}{' '.join(tokens[index:])}"
        return frame, opcode, length
    return None


def extract(input_path: Path) -> list[dict[str, object]]:
    selected: dict[tuple[str, int], dict[str, object]] = {}
    for line_number, line in enumerate(
        input_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        result = normalise_frame(line)
        if result is None:
            continue
        frame, opcode, length = result
        selected.setdefault(
            (opcode, length),
            {
                "opcode": opcode,
                "length": length,
                "hgi80": frame,
                "source_line": line_number,
            },
        )
    return [selected[key] for key in sorted(selected)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("ramses_rf/tests/fixtures/regression_packets_sorted.txt"),
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    cases = extract(args.input)
    opcodes = {case["opcode"] for case in cases}
    missing = sorted(SUPPORTED_OPCODES - opcodes)
    if missing:
        parser.error(f"input corpus has no inbound cases for: {', '.join(missing)}")

    output = json.dumps(cases, indent=2) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    print(
        f"Extracted {len(cases)} cases across {len(opcodes)} opcodes.", file=sys.stderr
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
