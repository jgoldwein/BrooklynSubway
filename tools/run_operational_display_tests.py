#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the Phase 4M memory-only operational display tests."""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "operational_display_test.exe"
        if h.build(h.ROOT / "tools/host_tests/operational_display_test.cpp", exe,
                   extra=(h.ROOT / "src/operational_display.cpp",)):
            return 1
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
