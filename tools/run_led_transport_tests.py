#!/usr/bin/env python3
"""Exercise the production RMT adapter against checked fake IDF calls."""
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/"led_transport_test.exe"
        command=h.compiler()+["-std=c++17","-Wall","-Wextra","-Werror","-O1",
            "-DBROOKLYN_TARGET_PCB=1","-DESP32=1",
            f"-I{h.ROOT/'tools/host_tests/led_transport_stubs'}",f"-I{h.ROOT/'include'}",
            str(h.ROOT/'tools/host_tests/led_transport_test.cpp'),
            str(h.ROOT/'src/led_transport_esp.cpp'),"-o",str(exe)]
        if subprocess.call(command): return 1
        for case in ("ok","core","configure","install","clock","route","write","wait"):
            if subprocess.call([str(exe),case]): return 1
    return 0

if __name__=="__main__": raise SystemExit(main())
