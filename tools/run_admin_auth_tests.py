#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run host-side administrator authentication tests."""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "admin_auth_test.exe"
        if h.build(
            h.ROOT / "tools/host_tests/admin_auth_test.cpp",
            exe,
            extra=[h.ROOT / "src/admin_auth.cpp"],
        ):
            return 1
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
