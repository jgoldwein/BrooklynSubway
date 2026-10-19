#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Run the host-only Phase 4Y GTFS-Realtime capture/replay tests."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "replay"))
import test_replay  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(test_replay.main())
