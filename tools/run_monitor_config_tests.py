#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import phase4e_host as h
sys.path.insert(0,str(h.ROOT))
from tools.telemetry.schema_validate import load_schema,validate

def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe=Path(tmp)/'monitor_config_test.exe'
        sources=['tools/host_tests/monitor_config_test.cpp','src/monitor_config.cpp','src/monitor_seed.cpp','src/telemetry_core.cpp','src/wan_address.cpp','src/lan_web_policy.cpp','src/oled_clock_time.cpp','src/admin_auth.cpp','src/network_diagnostics.cpp','src/normal_config.cpp','src/semantic_api.cpp','src/led_map.cpp','src/generated/semantic_tables.cpp']
        command=h.compiler()+['-std=c++17','-Wall','-Wextra','-Werror','-O1','-fno-exceptions',f'-I{h.ROOT / "include"}',*[str(h.ROOT / s) for s in sources],'-o',str(exe)]
        build=subprocess.run(command,capture_output=True,text=True)
        if build.returncode:print(build.stdout+build.stderr);return 1
        run=subprocess.run([str(exe)],capture_output=True,text=True)
        found=False
        for line in run.stdout.splitlines():
            if line.startswith('MONITOR_SAMPLE '):
                found=True
                errors=validate(json.loads(line.removeprefix('MONITOR_SAMPLE ')),load_schema('brooklyn-telemetry-v1.schema.json'))
                if errors:print(errors);return 1
            else:print(line)
        print(run.stderr,end='')
        return run.returncode or int(not found)
if __name__=='__main__':raise SystemExit(main())
