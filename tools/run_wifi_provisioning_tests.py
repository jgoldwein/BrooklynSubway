#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run the host-side Phase 4L Wi-Fi provisioning policy tests."""

import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402

_base_compiler = h.compiler


def _compiler_with_stdlib():
    cc = _base_compiler()
    if cc[1:3] == ["-m", "ziglang"]:
        return [cc[0], "-m", "ziglang", "c++"]
    return cc


h.compiler = _compiler_with_stdlib


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "wifi_provisioning_test.exe"
        if h.build(h.ROOT / "tools/host_tests/wifi_provisioning_test.cpp", exe,
                   extra=[h.ROOT / "src/feed_transport.cpp", h.ROOT / "src/wifi_policy.cpp"]):
            return 1
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
