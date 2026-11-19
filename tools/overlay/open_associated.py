# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Small detached Windows opener so editor logs stay out of the build console."""
import os
from pathlib import Path
import sys

if __name__=='__main__':
    try:
        os.startfile(str(Path(sys.argv[1]).resolve(strict=True)))
    except (OSError,AttributeError,IndexError) as error:
        print('Could not open associated application:',error,flush=True)
        raise SystemExit(1)
