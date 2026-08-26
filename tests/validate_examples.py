#!/usr/bin/env python3
"""Validate representative ESPHome YAML configuration contracts."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    esphome = root / ".venv/bin/esphome"
    command = [str(esphome)] if esphome.exists() else ["esphome"]
    examples = ("example-c6.yaml", "example-c6-devices.yaml", "example-c6-discovery.yaml")
    for example in examples:
        result = subprocess.run(
            [*command, "config", example],
            cwd=root,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode != 0:
            print(result.stdout)
            return result.returncode
        if "Configuration is valid!" not in result.stdout:
            print(f"{example} did not report a valid configuration")
            return 1
        print(f"Validated {example}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())