#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run memory-only Phase 4N output bridge host tests."""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "display_output_bridge_test.exe"
        command = h.compiler() + [
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-O1",
            "-fno-exceptions",
            "-DBROOKLYN_TARGET_XIAO=1",
            "-DBROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0",
            f"-I{h.ROOT / 'include'}",
            f"-I{h.ROOT / 'tools/host_tests'}",
            str(h.ROOT / "tools/host_tests/display_output_bridge_test.cpp"),
            str(h.ROOT / "src/display_output.cpp"),
            str(h.ROOT / "src/led_engine.cpp"),
            str(h.ROOT / "src/led_map.cpp"),
            str(h.ROOT / "tools/host_tests/led_power_stub.cpp"),
            "-o",
            str(exe),
        ]
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout, end="")
            print(build.stderr, end="")
            return build.returncode

        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())