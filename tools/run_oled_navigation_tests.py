#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "oled_navigation_test.exe"
        if h.build(h.ROOT / "tools/host_tests/oled_navigation_test.cpp", exe,
                   extra=[h.ROOT / "src/oled_navigation.cpp", h.ROOT / "src/weather.cpp", h.ROOT / "src/oled_screens.cpp"]):
            return 1
        return subprocess.call([str(exe)])

if __name__ == "__main__":
    raise SystemExit(main())
