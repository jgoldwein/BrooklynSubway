#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Run the offline, loopback-only Brooklyn virtual map simulator."""

import argparse
import json
import subprocess
import sys
import tempfile
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

if __package__:
    from .core import (
        GENERATED,
        HERE,
        build,
        check_generated,
        generated_metadata,
        run_renderer,
        write_generated,
    )
else:
    from core import GENERATED, HERE, build, check_generated, generated_metadata, run_renderer, write_generated

from replay.core import parse_utc, validate_replay_schema


def validate_render_request(request, metadata):
    if not isinstance(request, dict) or request.get("schemaVersion") != 1:
        raise ValueError("expected frame schemaVersion 1")
    train_frame = request.get("trainFrame")
    if not isinstance(train_frame, dict):
        raise ValueError("trainFrame occupancy metadata is required to rerender")
    occupancy = train_frame.get("occupancy")
    if not isinstance(occupancy, list) or len(occupancy) != 306:
        raise ValueError("trainFrame.occupancy must contain exactly 306 values")
    for index, value in enumerate(occupancy):
        if type(value) is not int or value < 0 or value > 255:
            raise ValueError(f"occupancy[{index}] must be an integer from 0 to 255")
        if value and metadata["pixels"][index]["semanticType"] == "STATUS":
            raise ValueError("status LEDs cannot have train occupancy")
    brightness = request.get("brightness")
    if type(brightness) is not int or not 0 <= brightness <= 255:
        raise ValueError("brightness must be an integer from 0 to 255")
    mask = request.get("routeVisibilityMask")
    if type(mask) is not int or mask < 0 or mask > 0x3FFFF:
        raise ValueError("routeVisibilityMask must be an 18-bit canonical mask")
    display_state = request.get("displayState")
    if not isinstance(display_state, str) or display_state not in {
        "WaitingForData", "Fresh", "DegradedHold", "DegradedPartial", "StaleHold", "NoData"
    }:
        raise ValueError("unknown operational display state")
    status = request.get("status", {})
    if not isinstance(status, dict):
        raise ValueError("status must be an object")
    wifi_state = status.get("wifiState", "Associated")
    if not isinstance(wifi_state, str) or wifi_state not in {
        "Disabled", "Unavailable", "Connecting", "Associated", "TimeSyncing"
    }:
        raise ValueError("unknown Wi-Fi indicator state")
    if type(status.get("systemHealthy", True)) is not bool or type(
        status.get("provisioning", False)
    ) is not bool:
        raise ValueError("status booleans must be JSON booleans")
    trains = train_frame.get("trains", [])
    if not isinstance(trains, list) or len(trains) > 1000:
        raise ValueError("trainFrame.trains must be an array of at most 1000 entries")
    for train in trains:
        if not isinstance(train, dict):
            raise ValueError("each train metadata entry must be an object")
        global_index = train.get("globalIndex")
        if type(global_index) is not int or not 0 <= global_index < 306:
            raise ValueError("train metadata globalIndex must be in 0..305")
        if metadata["pixels"][global_index]["semanticType"] == "STATUS":
            raise ValueError("train metadata cannot reference a status LED")
    return request


def load_replay_export(path, metadata):
    try:
        data = Path(path).read_bytes()
    except OSError as error:
        raise ValueError(f"cannot read replay export: {error}") from error
    if len(data) > 512 * 1024 * 1024:
        raise ValueError("replay export exceeds the 512 MiB limit")
    try:
        replay = json.loads(data)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise ValueError(f"replay export is invalid JSON: {error}") from error
    validate_replay_schema(replay)
    frames = replay.get("frames")
    if not isinstance(frames, list) or not frames or len(frames) > 10000:
        raise ValueError("replay export must contain 1..10000 frames")
    feed_statuses = {
        "SUCCESS", "HTTP_FAILURE", "DNS_CONNECT_FAILURE", "TIMEOUT", "OMITTED", "UNAVAILABLE"
    }
    for index, frame in enumerate(frames):
        if (
            not isinstance(frame, dict)
            or type(frame.get("cycleIndex")) is not int
            or frame["cycleIndex"] != index
        ):
            raise ValueError("replay frame indices must be sequential starting at zero")
        if (
            not isinstance(frame.get("recordedUtc"), str)
            or type(frame.get("captureElapsedMs")) is not int
            or frame["captureElapsedMs"] < 0
            or type(frame.get("simulatedElapsedMs")) is not int
            or frame["simulatedElapsedMs"] < 0
            or type(frame.get("replayUnix")) is not int
            or not 0 <= frame["replayUnix"] <= 0xFFFFFFFF
            or type(frame.get("frameTimestamp")) is not int
            or not 0 <= frame["frameTimestamp"] <= 0xFFFFFFFF
        ):
            raise ValueError(f"replay frame {index} timing metadata is invalid")
        parse_utc(frame["recordedUtc"], f"replay frame {index} recordedUtc")
        if not isinstance(frame.get("displayState"), str) or frame["displayState"] not in {
            "WaitingForData", "Fresh", "DegradedHold", "DegradedPartial", "StaleHold", "NoData"
        }:
            raise ValueError(f"replay frame {index} has an unsupported display state")
        feeds = frame.get("feeds")
        if (
            not isinstance(feeds, dict)
            or type(feeds.get("available")) is not int
            or not 0 <= feeds["available"] <= 8
            or feeds.get("expected") != 8
            or not isinstance(feeds.get("completeness"), str)
            or feeds["completeness"] not in {"COMPLETE", "PARTIAL", "NO_FEEDS"}
            or not isinstance(feeds.get("missing"), list)
            or any(not isinstance(feed_id, str) for feed_id in feeds["missing"])
            or not isinstance(feeds.get("statuses"), list)
            or len(feeds["statuses"]) != 8
            or any(
                not isinstance(slot, dict)
                or not isinstance(slot.get("feedId"), str)
                or not isinstance(slot.get("status"), str)
                or slot["status"] not in feed_statuses
                for slot in feeds["statuses"]
            )
        ):
            raise ValueError(f"replay frame {index} feed metadata is invalid")
        successful_feeds = sum(
            slot["status"] == "SUCCESS" for slot in feeds["statuses"]
        )
        expected_missing = [
            slot["feedId"] for slot in feeds["statuses"] if slot["status"] != "SUCCESS"
        ]
        if (
            len({slot["feedId"] for slot in feeds["statuses"]}) != 8
            or successful_feeds != feeds["available"]
            or expected_missing != feeds["missing"]
            or feeds["completeness"] != (
                "COMPLETE" if successful_feeds == 8
                else "NO_FEEDS" if successful_feeds == 0
                else "PARTIAL"
            )
        ):
            raise ValueError(f"replay frame {index} feed totals are inconsistent")
        pipeline = frame.get("pipeline")
        if (
            not isinstance(pipeline, dict)
            or any(
                type(pipeline.get(key)) is not int or pipeline[key] < 0
                for key in ("vehiclesSeen", "resolvedTrains", "feedDecodeFailures",
                            "occupiedLeds", "collisionLeds")
            )
        ):
            raise ValueError(f"replay frame {index} pipeline counters are invalid")
        pixels = frame.get("pixels")
        occupancy = frame.get("occupancy")
        if not isinstance(pixels, list) or len(pixels) != 306:
            raise ValueError(f"replay frame {index} must contain 306 pixels")
        if not isinstance(occupancy, list) or len(occupancy) != 306:
            raise ValueError(f"replay frame {index} must contain 306 occupancy entries")
        for pixel in pixels:
            if not isinstance(pixel, dict) or any(
                type(pixel.get(channel)) is not int or not 0 <= pixel[channel] <= 255
                for channel in ("r", "g", "b")
            ):
                raise ValueError(f"replay frame {index} contains invalid RGB")
        if any(type(count) is not int or not 0 <= count <= 255 for count in occupancy):
            raise ValueError(f"replay frame {index} contains invalid occupancy")
        trains = frame.get("trains")
        if not isinstance(trains, list) or len(trains) > 2048:
            raise ValueError(f"replay frame {index} train metadata is invalid")
        collisions = frame.get("collisions")
        if not isinstance(collisions, list) or len(collisions) > 306:
            raise ValueError(f"replay frame {index} collision list is invalid")
        try:
            validate_render_request(
                {
                    "schemaVersion": 1,
                    "trainFrame": {"occupancy": occupancy, "trains": trains},
                    "brightness": frame["brightness"],
                    "routeVisibilityMask": frame["routeVisibilityMask"],
                    "displayState": frame["displayState"],
                    "status": {
                        "systemHealthy": frame["systemHealthy"],
                        "wifiState": frame["wifiState"],
                        "provisioning": frame["provisioning"],
                    },
                },
                metadata,
            )
        except (KeyError, ValueError) as error:
            raise ValueError(f"replay frame {index} is invalid: {error}") from error
        for train in trains:
            if not isinstance(train, dict) or any(
                not isinstance(train.get(key), str)
                for key in ("route", "tripId", "vehicleId", "trainId", "currentStop",
                            "nextStop", "previousStop", "previousStopSource")
            ):
                raise ValueError(f"replay frame {index} contains invalid train metadata")
        observations = frame.get("observations", [])
        if not isinstance(observations, list) or len(observations) > 2048:
            raise ValueError(f"replay frame {index} observation metadata is invalid")
        for observation in observations:
            if not isinstance(observation, dict) or any(
                not isinstance(observation.get(key), str)
                for key in ("route", "tripId", "currentStop", "nextStop", "category",
                            "reason", "previousStop", "previousStopSource",
                            "previousStopFailure")
            ):
                raise ValueError(f"replay frame {index} contains invalid observation metadata")
        for collision in collisions:
            if (
                not isinstance(collision, dict)
                or type(collision.get("globalIndex")) is not int
                or not 0 <= collision["globalIndex"] < 306
                or metadata["pixels"][collision["globalIndex"]]["semanticType"] == "STATUS"
                or type(collision.get("trainCount")) is not int
                or collision["trainCount"] < 2
                or not isinstance(collision.get("trains"), list)
                or len(collision["trains"]) != collision["trainCount"]
                or occupancy[collision["globalIndex"]] != collision["trainCount"]
                or not isinstance(collision.get("designator"), str)
                or not isinstance(collision.get("route"), str)
                or not isinstance(collision.get("semanticType"), str)
                or collision["semanticType"] not in {"STATION", "SEGMENT"}
            ):
                raise ValueError(f"replay frame {index} contains invalid collision metadata")
            rgb = collision.get("rgb")
            if not isinstance(rgb, dict) or any(
                type(rgb.get(channel)) is not int or not 0 <= rgb[channel] <= 255
                for channel in ("r", "g", "b")
            ):
                raise ValueError(f"replay frame {index} contains invalid collision RGB")
            if rgb != pixels[collision["globalIndex"]]:
                raise ValueError(f"replay frame {index} collision RGB differs from its LED pixel")
    return data, replay


class Handler(BaseHTTPRequestHandler):
    server_version = "BrooklynVirtualMap/1"

    def do_GET(self):
        request_path = self.path.split("?", 1)[0]
        if request_path in ("/", "/index.html"):
            path = HERE / "index.html"
            content_type = "text/html; charset=utf-8"
        elif request_path == "/metadata.json":
            self.respond(200, self.server.metadata_blob, "application/json; charset=utf-8")
            return
        elif request_path == "/replay.json" and self.server.replay_blob:
            self.respond(200, self.server.replay_blob, "application/json; charset=utf-8")
            return
        else:
            self.respond(404, b"Not found\n", "text/plain; charset=utf-8")
            return
        self.respond(200, path.read_bytes(), content_type)

    def do_POST(self):
        if self.path != "/api/render":
            self.respond(404, b"Not found\n", "text/plain; charset=utf-8")
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if size <= 0 or size > 1_000_000:
                raise ValueError("request body must be between 1 byte and 1 MB")
            request = json.loads(self.rfile.read(size))
            validate_render_request(request, self.server.metadata)
            rendered = run_renderer(self.server.executable, request)
            data = json.dumps(rendered, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            self.respond(200, data, "application/json; charset=utf-8")
        except (ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
            message = json.dumps({"error": str(error)}).encode("utf-8")
            self.respond(400, message, "application/json; charset=utf-8")

    def respond(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print("%s - %s" % (self.client_address[0], fmt % args))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--export", action="store_true", help="regenerate metadata and exit")
    parser.add_argument("--check", action="store_true", help="check generated metadata and exit")
    parser.add_argument("--replay-json", type=Path, help="load a validated Phase 4Y replay export")
    parser.add_argument("--replay-paused", action="store_true", help="start an imported replay paused")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("port must be in 0..65535")

    with tempfile.TemporaryDirectory(prefix="brooklyn-virtual-map-") as temporary:
        executable = Path(temporary) / "virtual_map_host.exe"
        build(executable)
        if args.check:
            check_generated(executable)
            print("Generated simulator metadata is current.")
            return 0
        if args.export:
            write_generated(executable)
            print("Wrote", GENERATED)
            return 0
        write_generated(executable)
        metadata = generated_metadata(executable)
        metadata_blob = GENERATED.read_bytes()
        replay_blob = b""
        replay = None
        if args.replay_json is not None:
            replay_blob, replay = load_replay_export(args.replay_json, metadata)
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
        server.executable = executable
        server.metadata = metadata
        server.metadata_blob = metadata_blob
        server.replay_blob = replay_blob
        url = "http://127.0.0.1:%d/" % server.server_address[1]
        if replay is not None and not args.replay_paused:
            url += "?autoplay=1"
        print("Virtual Brooklyn Map: " + url, flush=True)
        print("Localhost only. Press Ctrl+C to stop.", flush=True)
        if replay is not None:
            print(
                f"Loaded replay session {replay.get('captureSessionId', 'unknown')} "
                f"({len(replay['frames'])} frames).",
                flush=True,
            )
        if not args.no_browser:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            print("\nStopping virtual map server.")
        finally:
            server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
