#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Verify and deterministically replay a recorded raw MTA GTFS-RT session."""

import argparse
from collections import Counter
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from core import (
    HERE,
    build_bridge,
    bridge_feeds,
    canonical_json,
    export_replay,
    load_manifest,
    run_bridge,
)


def select_cycles(manifest, cycle_number, replay_all):
    if cycle_number is not None:
        if cycle_number < 1 or cycle_number > len(manifest["cycles"]):
            raise ValueError(f"cycle must be in 1..{len(manifest['cycles'])}")
        selected = dict(manifest)
        selected["cycles"] = manifest["cycles"][:cycle_number]
        return selected, cycle_number
    if replay_all:
        return manifest, len(manifest["cycles"])
    selected = dict(manifest)
    selected["cycles"] = manifest["cycles"][:1]
    return selected, 1


def print_summary(replay):
    unresolved = Counter()
    for frame in replay["frames"]:
        feed_summary = f'{frame["feeds"]["available"]}/8'
        missing = ",".join(frame["feeds"]["missing"]) or "none"
        counters = frame["pipeline"]
        payload_bytes = sum(
            slot.get("bytes", 0)
            for cycle in replay["_cycles"]
            if cycle["index"] == frame["cycleIndex"]
            for slot in cycle["feeds"]
            if slot["status"] == "SUCCESS"
        )
        digest = hashlib.sha256(canonical_json(frame)).hexdigest()
        print(
            f'cycle {frame["cycleIndex"] + 1}: feeds {feed_summary} '
            f'(missing {missing}), {payload_bytes} bytes, '
            f'{counters["vehiclesSeen"]} vehicles, '
            f'{counters["resolvedTrains"]} resolved, '
            f'{counters["occupiedLeds"]} occupied LEDs, '
            f'{counters["collisionLeds"]} collisions, '
            f'{frame["displayState"]}, replay t={frame["simulatedElapsedMs"]} ms, '
            f'frame SHA-256 {digest}'
        )
        if counters["feedDecodeFailures"]:
            print(f'  parse failures: {counters["feedDecodeFailures"]}')
        if counters["unknownStop"] or counters["invalid"] or counters["previousStopAmbiguous"]:
            print(
                "  unresolved: "
                f'unknown stops {counters["unknownStop"]}, invalid {counters["invalid"]}, '
                f'ambiguous previous stop {counters["previousStopAmbiguous"]}'
            )
        for item in frame.get("observations", []):
            unresolved[
                (
                    item["category"],
                    item["route"],
                    item["currentStop"],
                    item["reason"],
                    item["previousStopFailure"],
                )
            ] += 1
        for collision in frame["collisions"]:
            print(
                f'  collision {collision["designator"]}: {collision["trainCount"]} trains, '
                f'route {collision["route"]}, RGB {collision["rgb"]}'
            )
    if unresolved:
        print("Unresolved supported-route observations (category, route, stop, reason, previous-stop failure):")
        for (category, route, stop, reason, previous_failure), count in unresolved.most_common(30):
            print(
                f"  {count:4d}  {category}  route {route}  stop {stop}  "
                f"{reason or 'None'}  previous-stop {previous_failure}"
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--cycle", type=int, help="replay through this one-based cycle number")
    mode.add_argument("--all", action="store_true", help="replay every recorded cycle")
    parser.add_argument("--export", type=Path, help="write canonical replay JSON to this path")
    parser.add_argument("--serve", action="store_true", help="open the selected replay in the simulator")
    parser.add_argument("--speed", type=float, default=1.0, help="scale recorded cycle timing (default 1)")
    parser.add_argument("--step", action="store_true", help="start simulator paused for manual step controls")
    parser.add_argument("--brightness", type=int, default=64)
    parser.add_argument("--route-mask", type=lambda value: int(value, 0), default=0x3FFFF)
    parser.add_argument("--omit-feed", action="append", default=[], metavar="FEED")
    parser.add_argument("--no-feeds", action="store_true")
    parser.add_argument("--delay-next-cycle-ms", type=int, default=0)
    parser.add_argument("--corrupt-feed", metavar="FEED")
    args = parser.parse_args()
    if args.speed <= 0 or args.speed > 1000:
        parser.error("--speed must be greater than zero and at most 1000")
    if args.delay_next_cycle_ms < 0 or args.delay_next_cycle_ms > 0xFFFFFFFF:
        parser.error("--delay-next-cycle-ms must fit uint32 milliseconds")

    try:
        with tempfile.TemporaryDirectory(prefix="brooklyn-gtfsrt-replay-") as temporary:
            executable = Path(temporary) / "brooklyn_replay_host.exe"
            build_bridge(executable)
            feeds = bridge_feeds(executable)
            manifest = load_manifest(args.session, feeds)
            selected, _ = select_cycles(manifest, args.cycle, args.all)
            replay = run_bridge(
                executable,
                selected,
                args.brightness,
                args.route_mask,
                args.speed,
                args.omit_feed,
                args.no_feeds,
                args.corrupt_feed,
                args.delay_next_cycle_ms,
            )
            replay["_cycles"] = selected["cycles"]
            print_summary(replay)
            replay.pop("_cycles")
            if args.export:
                export_replay(args.export, replay)
                print(f"Replay export: {args.export.resolve()}")
            if args.serve:
                replay_path = Path(temporary) / "replay.json"
                export_replay(replay_path, replay)
                simulator = HERE.parent / "virtual_map" / "run.py"
                command = [
                    sys.executable,
                    str(simulator),
                    "--replay-json",
                    str(replay_path),
                ]
                if args.step:
                    command.append("--replay-paused")
                return subprocess.call(command)
            return 0
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        parser.exit(1, f"replay failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
