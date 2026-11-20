#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as folder:
        binary=Path(folder)/"wan_address_test.exe"
        if h.build(h.ROOT/"tools/host_tests/wan_address_test.cpp",binary,
                   extra=[h.ROOT/"src/wan_address.cpp"]):return 1
        return subprocess.call([str(binary)])

if __name__=="__main__":raise SystemExit(main())
