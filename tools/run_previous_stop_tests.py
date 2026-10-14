#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the host-side Phase 4F previous-stop tests (no hardware, no network).

The exhaustive static comparison needs the host pattern index
(local_reference/phase4e/pattern_index.tsv, built by
tools/phase4e_build_pattern_index.py from the official static GTFS). When it is
absent that part is skipped and reported as skipped.

Usage: python tools/run_previous_stop_tests.py
"""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    index = h.ROOT / "local_reference/phase4e/pattern_index.tsv"
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "previous_stop_test.exe"
        if h.build(h.ROOT / "tools/host_tests/previous_stop_test.cpp", exe):
            return 1
        args = [str(exe)] + ([str(index)] if index.exists() else [])
        run = subprocess.run(args, capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
