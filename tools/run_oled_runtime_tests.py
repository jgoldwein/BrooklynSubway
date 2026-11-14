#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run host tests for OLED startup isolation and bounded transport failures."""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "oled_runtime_test.exe"
        command = h.compiler() + [
            "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1",
            "-DBROOKLYN_TARGET_PCB=1",
            f"-I{h.ROOT / 'tools/host_tests/oled_stubs'}", f"-I{h.ROOT / 'include'}",
            str(h.ROOT / "tools/host_tests/oled_runtime_test.cpp"),
            str(h.ROOT / "src/oled_clock.cpp"),
            str(h.ROOT / "src/oled_clock_time.cpp"), "-o", str(exe),
        ]
        build = subprocess.run(command, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout, end="")
            print(build.stderr, end="")
            return build.returncode

        for scenario in range(24):
            run = subprocess.run([str(exe), str(scenario)], capture_output=True, text=True)
            print(run.stdout, end="")
            print(run.stderr, end="")
            if run.returncode:
                return run.returncode
        return 0



if __name__ == "__main__":
    sys.exit(main())
