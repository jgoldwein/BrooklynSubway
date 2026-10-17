#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run host tests for normal configuration and LAN web rendering."""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "lan_web_policy_test.exe"
        command = h.compiler() + [
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-O1",
            "-fno-exceptions",
            f"-I{h.ROOT / 'include'}",
            str(h.ROOT / "tools/host_tests/lan_web_policy_test.cpp"),
            str(h.ROOT / "src/lan_web_policy.cpp"),
            str(h.ROOT / "src/normal_config.cpp"),
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
