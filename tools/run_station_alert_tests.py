# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import subprocess,sys,tempfile
from pathlib import Path
import phase4e_host as h
def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/'station_alerts.exe'
        r=subprocess.run(h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror','-O1','-UNDEBUG','-I'+str(h.ROOT/'include'),str(h.ROOT/'tools/host_tests/station_alerts_test.cpp'),str(h.ROOT/'src/station_alerts.cpp'),'-o',str(exe)])
        if r.returncode:return r.returncode
        result=subprocess.call([str(exe),*sys.argv[1:]])
        if result or len(sys.argv)>1:return result
        command=h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror','-DARDUINO_ARCH_ESP32',
            '-I'+str(h.ROOT/'tools/host_tests/personal_stubs'),'-I'+str(h.ROOT/'include'),
            str(h.ROOT/'tools/host_tests/station_alerts_store_test.cpp'),str(h.ROOT/'src/station_alerts_esp.cpp'),'-o',str(exe)]
        r=subprocess.run(command)
        return r.returncode or subprocess.call([str(exe)])
if __name__=='__main__':raise SystemExit(main())
