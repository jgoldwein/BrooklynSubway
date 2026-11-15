#!/usr/bin/env python3
"""Read-only collection of actual commanded map indications (firmware >=0.2.47).

Example: python tools/monitor_indications.py --url http://10.1.1.56 --seconds 180
History is RAM-only, LED-level (not individual train dwell), and may be overwritten.
The collector follows sequence cursors and explicitly logs gaps and restarts.
"""
import argparse
import datetime
import json
from pathlib import Path
import time
import urllib.parse
import urllib.request


class Collector:
    def __init__(self):
        self.cursor = 0
        self.uptime = None

    def collect(self, request, max_pages=8):
        records = []
        for _ in range(max_pages):
            report = request(self.cursor)
            if report.get("schemaVersion") != 1:
                raise ValueError("Unsupported indication schema; install firmware 0.2.47 or newer")
            now = report["uptimeMs"]
            rollover = self.uptime is not None and self.uptime > 0xffff0000 and now < 60000
            if self.uptime is not None and now < self.uptime and not rollover:
                records.append({"type": "restart", "previousUptimeMs": self.uptime, "uptimeMs": now})
                self.cursor = 0
                self.uptime = now
                continue  # Fetch from the new boot's retained history, not the old cursor.
            self.uptime = now
            if report["historyGap"]:
                records.append({"type": "history-gap", "after": self.cursor,
                                "oldestSequence": report["oldestSequence"]})
            for event in report["events"]:
                if event["sequence"] > self.cursor:
                    records.append({"type": "ended", **event})
            self.cursor = max(self.cursor, report["nextAfter"])
            if not report["more"]:
                records.append({"type": "sample", **report})
                break
        return records


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--url", default="http://brooklynmap.local")
    ap.add_argument("--seconds", type=float, default=180)
    ap.add_argument("--interval", type=float, default=1)
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    if urllib.parse.urlsplit(args.url).scheme not in ("http", "https"):
        ap.error("Use an HTTP or HTTPS device URL")
    if not 0 < args.seconds <= 86400 or args.interval < 0.25:
        ap.error("Use 0 < seconds <= 86400 and interval >= 0.25")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = args.output or Path("captures") / ("indications-" + stamp + ".jsonl")
    output.parent.mkdir(parents=True, exist_ok=True)
    collector = Collector()
    end = time.monotonic() + args.seconds
    totals = {"samples": 0, "endings": 0, "gaps": 0, "restarts": 0, "errors": 0}

    def request(after):
        url = args.url.rstrip("/") + "/api/indications?after=" + str(after)
        with urllib.request.urlopen(url, timeout=5) as response:
            return json.load(response)

    try:
        with output.open("w", encoding="utf-8") as stream:
            while time.monotonic() < end:
                try:
                    records = collector.collect(request)
                except Exception as error:
                    records = [{"type": "error", "message": str(error)}]
                for record in records:
                    record["observedUtc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
                    stream.write(json.dumps(record) + "\n")
                    kind = record["type"]
                    key = {"sample": "samples", "ended": "endings", "history-gap": "gaps", "restart": "restarts", "error": "errors"}[kind]
                    totals[key] += 1
                    if kind == "ended":
                        print(record["line"], record["station"], "LED" + str(record["led"]),
                              str(record["durationMs"]) + " ms", record["reason"], flush=True)
                    elif kind != "sample":
                        print(json.dumps(record), flush=True)
                stream.flush()
                time.sleep(min(args.interval, max(0, end-time.monotonic())))
    except KeyboardInterrupt:
        pass
    print(json.dumps({"output": str(output), **totals}), flush=True)
    return 1 if totals["errors"] and not totals["samples"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
