import subprocess,tempfile
from pathlib import Path
import phase4e_host as h
with tempfile.TemporaryDirectory() as tmp:
    exe=Path(tmp)/'feed_stream_reader_test.exe'
    result=h.build(h.ROOT/'tools/host_tests/feed_stream_reader_test.cpp',exe)
    raise SystemExit(result or subprocess.call([str(exe)]))
