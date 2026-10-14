#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the host-side Phase 4C resolver tests (no hardware).

Needs a host C++17 compiler: g++/clang++ on PATH, or `pip install ziglang`
(used via `python -m ziglang cc -x c++`, which avoids building libc++).

Usage: python tools/run_resolver_tests.py
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES = [
    ROOT / "tools/host_tests/resolver_test.cpp",
    ROOT / "src/train_position_resolver.cpp",
    ROOT / "src/semantic_api.cpp",
    ROOT / "src/generated/semantic_tables.cpp",
    ROOT / "src/led_map.cpp",
]


def compiler():
    for name in ("g++", "clang++"):
        if shutil.which(name):
            return [name]
    probe = subprocess.run([sys.executable, "-m", "ziglang", "version"],
                           capture_output=True, text=True)
    if probe.returncode == 0:
        return [sys.executable, "-m", "ziglang", "cc", "-x", "c++"]
    sys.exit("no host C++ compiler found (install g++/clang++ or `pip install ziglang`)")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "resolver_test.exe"
        cmd = compiler() + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1",
                            f"-I{ROOT / 'include'}", *map(str, SOURCES), "-o", str(exe)]
        build = subprocess.run(cmd, capture_output=True, text=True)
        if build.returncode != 0:
            print(build.stdout + build.stderr)
            return build.returncode
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
