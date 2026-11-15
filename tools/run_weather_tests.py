#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/"weather_test.exe"
        if h.build(h.ROOT/"tools/host_tests/weather_test.cpp",exe,
                   extra=[h.ROOT/"src/weather.cpp",h.ROOT/"src/oled_screens.cpp"]):
            return 1
        return subprocess.call([str(exe)])

if __name__=="__main__":
    raise SystemExit(main())
