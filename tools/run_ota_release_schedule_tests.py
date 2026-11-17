# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import subprocess
import tempfile
import phase4e_host as h
def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/'ota_schedule.exe'
        r=subprocess.run(h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror',
            '-I'+str(h.ROOT/'include'),str(h.ROOT/'tools/host_tests/ota_release_schedule_test.cpp'),'-o',str(exe)])
        return r.returncode or subprocess.call([str(exe)])
if __name__=='__main__':
    raise SystemExit(main())
