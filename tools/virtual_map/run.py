#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Run the offline, loopback-only Brooklyn virtual map simulator."""

import argparse
import json
import subprocess
import tempfile
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from core import GENERATED, HERE, build, check_generated, generated_metadata, run_renderer, write_generated


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
    if request.get("displayState") not in {
        "WaitingForData", "Fresh", "DegradedHold", "DegradedPartial", "StaleHold", "NoData"
    }:
        raise ValueError("unknown operational display state")
    status = request.get("status", {})
    if not isinstance(status, dict):
        raise ValueError("status must be an object")
    if status.get("wifiState", "Associated") not in {
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


class Handler(BaseHTTPRequestHandler):
    server_version = "BrooklynVirtualMap/1"

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            path = HERE / "index.html"
            content_type = "text/html; charset=utf-8"
        elif self.path == "/metadata.json":
            self.respond(200, self.server.metadata_blob, "application/json; charset=utf-8")
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
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
        server.executable = executable
        server.metadata = metadata
        server.metadata_blob = metadata_blob
        url = "http://127.0.0.1:%d/" % server.server_address[1]
        print("Virtual Brooklyn Map: " + url, flush=True)
        print("Localhost only. Press Ctrl+C to stop.", flush=True)
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
