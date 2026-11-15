#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path
if __name__ == "__main__":
    raise SystemExit(subprocess.call([sys.executable,str(Path(__file__).parent/"host_tests"/"test_indication_collector.py")]))
