#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "controls_test.exe"
        cmd = h.compiler() + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1",
            "-DBROOKLYN_TARGET_PCB=1", f"-I{h.ROOT / 'include'}",
            f"-I{h.ROOT / 'tools/host_tests/led_diagnostic_stubs'}",
            str(h.ROOT / "tools/host_tests/controls_test.cpp"),
            str(h.ROOT / "src/controls.cpp"), str(h.ROOT / "src/wifi_policy.cpp"),
            "-o", str(exe)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode:
            print(result.stdout + result.stderr)
            return result.returncode
        return subprocess.call([str(exe)])

if __name__ == "__main__":
    raise SystemExit(main())
