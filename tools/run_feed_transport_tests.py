#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the host-side Phase 4J transport tests (no hardware, no network).

Usage: python tools/run_feed_transport_tests.py [--replay DIR[@UNIX_NOW]]

--replay serves the eight recorded raw feeds in DIR (*.pb, glob order = the
configured feed order) through the transport core into the real pipeline.
ESP32 Wi-Fi/TLS is not exercised here.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402

# Feed order of src/network_config.h (kFeeds).
ORDER = ["feed.pb", "feed-ace.pb", "feed-bdfm.pb", "feed-g.pb",
         "feed-jz.pb", "feed-nqrw.pb", "feed-l.pb", "feed-si.pb"]


_base_compiler = h.compiler


def _compiler_with_stdlib():
    # The test uses <vector>; `zig cc -x c++` has no C++ standard library, `zig c++` does.
    cc = _base_compiler()
    if cc[1:3] == ["-m", "ziglang"]:
        return [cc[0], "-m", "ziglang", "c++"]
    return cc


h.compiler = _compiler_with_stdlib


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--replay", default=None)
    args = ap.parse_args()
    cmd_args = []
    if args.replay:
        directory, _, now = args.replay.partition("@")
        files = [Path(directory) / n for n in ORDER]
        missing = [str(f) for f in files if not f.exists()]
        if missing:
            print("missing feeds:", missing)
            return 1
        cmd_args = [now or "0", *map(str, files)]
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "feed_transport_test.exe"
        if h.build(h.ROOT / "tools/host_tests/feed_transport_test.cpp", exe,
                   extra=[h.ROOT / "src/feed_transport.cpp"]):
            return 1
        run = subprocess.run([str(exe), *cmd_args], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
