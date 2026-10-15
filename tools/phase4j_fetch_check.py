#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Phase 4J host cross-check: fetch the eight MTA feeds with HTTP status/size reporting.

Host only. This validates the payloads and the feed-buffer sizing, NOT ESP32 TLS.
Feed URLs here must stay identical to the table in src/network_config.cpp.

Usage: python tools/phase4j_fetch_check.py [--out DIR] [--capacity BYTES] [--sizes-of DIR ...]
Default DIR: local_reference/phase4j/live (git-ignored).
"""

import argparse
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

BASE = "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs"
SUFFIXES = ["", "-ace", "-bdfm", "-g", "-jz", "-nqrw", "-l", "-si"]
ROOT = Path(__file__).resolve().parent.parent


def fetch(suffix):
    req = urllib.request.Request(BASE + suffix,
                                 headers={"User-Agent": "BrooklynActiveSubwayMap-host-tool"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.status, r.read(), r.headers.get("Content-Length"), r.headers.get("Transfer-Encoding")
    except urllib.error.HTTPError as e:
        return e.code, b"", None, None


def historical_sizes(dirs):
    sizes = []
    for d in dirs:
        for p in Path(d).rglob("*.pb"):
            sizes.append((p.stat().st_size, str(p)))
    return sizes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ROOT / "local_reference/phase4j/live")
    ap.add_argument("--capacity", type=int, default=393216)
    ap.add_argument("--sizes-of", type=Path, nargs="*", default=[])
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    start = datetime.now(timezone.utc)
    sizes = []
    bad = 0
    for suffix in SUFFIXES:
        name = f"feed{suffix}.pb"
        try:
            status, data, clen, te = fetch(suffix)
        except Exception as e:  # network errors are reported, never hidden
            print(f"FAIL {name:14} {e}")
            bad += 1
            continue
        if status == 200 and data:
            (args.out / name).write_bytes(data)
            sizes.append((len(data), name))
        else:
            bad += 1
        print(f"HTTP {status} {name:14} {len(data):8} B  Content-Length={clen} Transfer-Encoding={te}")
    end = datetime.now(timezone.utc)
    ny = ZoneInfo("America/New_York")
    print(f"observed {start.astimezone(ny):%Y-%m-%d %H:%M:%S}-{end.astimezone(ny):%H:%M:%S} America/New_York"
          f" (UTC {start:%H:%M:%S}-{end:%H:%M:%S}, unix {int(start.timestamp())})")
    if sizes:
        total = sum(s for s, _ in sizes)
        print(f"feeds={len(sizes)} total={total} B largest={max(sizes)[0]} ({max(sizes)[1]})"
              f" smallest={min(sizes)[0]} ({min(sizes)[1]})")
        over = [n for s, n in sizes if s > args.capacity]
        print(f"capacity {args.capacity} B: {len(over)} feed(s) exceed" + (f" {over}" if over else ""))
    hist = historical_sizes(args.sizes_of)
    if hist:
        big = max(hist)
        print(f"historical: {len(hist)} feed files, largest {big[0]} B ({big[1]}), smallest {min(hist)[0]} B")
        print(f"historical: margin at capacity {args.capacity} = {args.capacity - big[0]} B "
              f"({args.capacity / big[0]:.2f}x)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
