import subprocess
import tempfile
from pathlib import Path
import phase4e_host as h

with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / "network_recovery_test.exe"
    result = h.build(h.ROOT / "tools/host_tests/network_recovery_test.cpp", exe)
    raise SystemExit(result or subprocess.call([str(exe)]))
