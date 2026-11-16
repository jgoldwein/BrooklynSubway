#!/usr/bin/env python3
import subprocess,tempfile
from pathlib import Path
import phase4e_host as h

def main():
 with tempfile.TemporaryDirectory() as d:
  exe=Path(d)/'tls_memory.exe'
  command=h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror','-O1','-DBROOKLYN_TARGET_PCB=1','-DBROOKLYN_NETWORK_ENABLED=1',f'-I{h.ROOT/"tools/host_tests/tls_memory_stubs"}',f'-I{h.ROOT/"include"}',str(h.ROOT/'tools/host_tests/tls_memory_test.cpp'),str(h.ROOT/'src/tls_memory.cpp'),'-o',str(exe)]
  result=subprocess.run(command)
  return result.returncode if result.returncode else subprocess.call([str(exe)])
if __name__=='__main__':raise SystemExit(main())
