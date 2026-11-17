# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/'personalization.exe'
        command=h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror','-DARDUINO_ARCH_ESP32',
            '-I'+str(h.ROOT/'tools/host_tests/personal_stubs'),'-I'+str(h.ROOT/'include'),
            str(h.ROOT/'tools/host_tests/personalization_store_test.cpp'),
            str(h.ROOT/'src/personalization.cpp'),'-o',str(exe)]
        build=subprocess.run(command)
        return build.returncode or subprocess.call([str(exe)])

if __name__=='__main__':
    raise SystemExit(main())
