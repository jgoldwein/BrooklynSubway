#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Download the MTA subway GTFS-RT snapshots (host only, no API key needed).

Feeds are verified empirically (see docs/PHASE4E_GTFS_RT_INGESTION.md).
Output files are raw protobuf FeedMessages: feed.pb, feed-ace.pb, ...
They are NOT meant to be committed.

Usage: python tools/phase4e_fetch_feeds.py [--out DIR] [--static]
Default DIR: local_reference/phase4e/feeds (git-ignored).
--static also downloads the static GTFS zip next to it.
"""

import argparse
import sys
import urllib.request
from pathlib import Path

BASE = "https://api-endpoint.mta.info/Dataservice/mtagtfsfeeds/nyct%2Fgtfs"
# suffix -> routes observed in the feed
FEEDS = {
    "": "1 2 3 4 5 6 7 GS",
    "-ace": "A C E H",
    "-bdfm": "B D F M FS",
    "-g": "G",
    "-jz": "J Z",
    "-nqrw": "N Q R W",
    "-l": "L",
    "-si": "SI SS",
}
STATIC_URL = "https://rrgtfsfeeds.s3.amazonaws.com/gtfs_subway.zip"
ROOT = Path(__file__).resolve().parent.parent


def fetch(url, dest):
    req = urllib.request.Request(url, headers={"User-Agent": "BrooklynActiveSubwayMap-host-tool"})
    with urllib.request.urlopen(req, timeout=30) as r:
        data = r.read()
    dest.write_bytes(data)
    return len(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ROOT / "local_reference/phase4e/feeds")
    ap.add_argument("--static", action="store_true")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    failed = 0
    for suffix, routes in FEEDS.items():
        name = f"feed{suffix}.pb"
        try:
            n = fetch(BASE + suffix, args.out / name)
            print(f"OK   {name:14} {n:8} B  routes: {routes}")
        except Exception as e:  # network errors are reported, never hidden
            failed += 1
            print(f"FAIL {name:14} {e}")
    if args.static:
        try:
            n = fetch(STATIC_URL, args.out / "gtfs_subway.zip")
            print(f"OK   gtfs_subway.zip {n} B")
        except Exception as e:
            failed += 1
            print(f"FAIL static GTFS: {e}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
