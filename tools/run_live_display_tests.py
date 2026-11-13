#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "live_display_test.exe"
        if h.build(h.ROOT / "tools/host_tests/live_display_test.cpp", exe,
                   extra=(h.ROOT / "src/live_display_policy.cpp",
                          h.ROOT / "src/operational_display.cpp",
                          h.ROOT / "src/led_power.cpp")):
            return 1
        return subprocess.call([str(exe)])

if __name__ == "__main__":
    raise SystemExit(main())
