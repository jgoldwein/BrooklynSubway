#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Phase 4E host diagnostic: GTFS-RT snapshot -> adapter -> resolver table.

Usage: python tools/phase4e_diag.py [--patterns TSV] [--route R] [--category C]
           [--max-age S] [--now T] [--limit N] [--all] [--verbose]
           [--summary-only] feed.pb [feed2.pb ...]
With no feed files, uses local_reference/phase4e/feeds/*.pb and the pattern
index local_reference/phase4e/pattern_index.tsv when present.
Output is a concise table, not a raw protobuf dump. No hardware is touched.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402


def main():
    args = sys.argv[1:]
    default_dir = h.ROOT / "local_reference/phase4e"
    if "--patterns" not in args and (default_dir / "pattern_index.tsv").exists():
        args += ["--patterns", str(default_dir / "pattern_index.tsv")]
    if not any(a.endswith(".pb") for a in args):
        args += [str(p) for p in sorted((default_dir / "feeds").glob("*.pb"))]
    return h.build_and_run(h.ROOT / "tools/host_tests/gtfs_rt_diag.cpp", args)


if __name__ == "__main__":
    sys.exit(main())
