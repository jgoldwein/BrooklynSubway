# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Keep a readable console and a persistent copy of all launcher output."""
from datetime import datetime
import os
from pathlib import Path
import subprocess
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]


def main(argv=None):
    log_path=ROOT/'Overlay-launch.log'
    print('Launcher output is saved in:\n'+str(log_path),flush=True)
    environment=dict(os.environ,PYTHONUNBUFFERED='1',PYTHONIOENCODING='utf-8')
    if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(errors='replace')
    with log_path.open('w',encoding='utf-8') as log:
        log.write('Brooklyn Overlay Builder started '+datetime.now().isoformat(timespec='seconds')+'\n')
        log.flush()
        try:
            with subprocess.Popen([sys.executable,str(HERE/'start.py'),*(argv or [])],
                cwd=ROOT,env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                text=True,encoding='utf-8',errors='replace',bufsize=1) as child:
                for line in child.stdout:
                    print(line,end='',flush=True);log.write(line);log.flush()
                code=child.wait()
        except OSError as error:
            message='Could not start the builder: '+str(error)+'\n'
            print(message,end='',flush=True);log.write(message);code=1
        message='\nLauncher finished with exit code '+str(code)+'.\n'
        print(message,end='',flush=True);log.write(message)
    return code


if __name__=='__main__':raise SystemExit(main(sys.argv[1:]))
