#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Run Phase 4X virtual-map host tests."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "virtual_map"))
import test_virtual_map  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(test_virtual_map.main())
