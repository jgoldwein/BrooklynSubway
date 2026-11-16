import subprocess,tempfile
from pathlib import Path
import phase4e_host as h
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'service_sound_test.exe'
    result=h.build(h.ROOT/'tools/host_tests/service_sound_test.cpp',exe)
    raise SystemExit(result or subprocess.call([str(exe)]))
