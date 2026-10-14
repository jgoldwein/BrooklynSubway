#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the host-side Phase 4I pipeline tests (no hardware, no network).

Usage: python tools/run_train_pipeline_tests.py [--replay DIR[@UNIX_NOW]]...

Each --replay DIR is a directory of recorded raw GTFS-RT feed files (*.pb)
that is replayed through the pipeline and compared with the merged host path.
Without --replay only the synthetic tests run.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--replay", action="append", default=[])
    args = ap.parse_args()
    cmd_args = []
    for spec in args.replay:
        directory, _, now = spec.partition("@")
        files = sorted(Path(directory).glob("*.pb"))
        if not files:
            print(f"no feeds in {directory}")
            return 1
        cmd_args += [f"{Path(directory).name}@{now or 0}", *map(str, files), "|"]
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "train_pipeline_test.exe"
        if h.build(h.ROOT / "tools/host_tests/train_pipeline_test.cpp", exe):
            return 1
        run = subprocess.run([str(exe), *cmd_args], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
