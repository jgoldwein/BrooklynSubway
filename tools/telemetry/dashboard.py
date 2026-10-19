# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Local read-only monitoring dashboard (stdlib http.server, loopback only).

This serves unauthenticated pages and must stay bound to 127.0.0.1. A public
deployment (for example https://monitor.goldwein.net/) must sit behind an
authenticating reverse proxy; the auth model is intentionally undecided.

    python tools/telemetry/dashboard.py --db telemetry.sqlite3 --port 8780
"""

import argparse
import html
import json
import sqlite3
import sys
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    __package__ = "telemetry"

from .ingest import Collector, DEFAULT_INTERVAL_S, describe_device  # noqa: E402,F401
from .store import Store  # noqa: E402

CSP = "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'"
STATE_CLASS = {"ONLINE": "ok", "STALE": "warn", "OFFLINE": "bad", "UNKNOWN": "dim"}
HEALTH_CLASS = {"HEALTHY": "ok", "WARNING": "warn", "FAULT": "bad", "UNKNOWN": "dim"}


def _e(value):
    return html.escape("-" if value is None else str(value), quote=True)


def _age(seconds):
    if seconds is None:
        return "-"
    seconds = int(seconds)
    if seconds < 120:
        return f"{seconds}s"
    if seconds < 7200:
        return f"{seconds // 60}m"
    if seconds < 172800:
        return f"{seconds // 3600}h"
    return f"{seconds // 86400}d"


def _uptime(ms):
    seconds = ms // 1000
    return f"{seconds // 86400}d {seconds % 86400 // 3600}h {seconds % 3600 // 60}m"


def _mask(value):
    return "-" if value is None else f"0x{int(value):08X}"


def _badge(text, mapping):
    return f'<span class="b {mapping.get(text, "dim")}">{_e(text)}</span>'


def _device_row(device):
    s = device["status"]
    cells = [
        f"<td>{_e(device['device_name'])}<br><small>{_e(device['device_id'])}</small></td>",
        f"<td>{_badge(device['state'], STATE_CLASS)}<br>{_badge(device['health'], HEALTH_CLASS)}</td>",
        f"<td>{_age(device['status_age_s'])}"
        + ("<br><small>retained only</small>" if device["status_is_retained_only"] else "")
        + f"<br><small>avail: {_e(device['availability'])}</small></td>",
    ]
    try:
        cells += _status_cells(s) if s else ['<td colspan="7"><small>no heartbeat received</small></td>']
    except (KeyError, TypeError, ValueError):
        cells.append('<td colspan="7"><small>stored status unreadable</small></td>')
    return "<tr>" + "".join(cells) + "</tr>"


def _status_cells(s):
    fw, sysm, net, mta, disp, cfg, saf = (
        s["firmware"], s["system"], s["network"], s["mta"], s["display"],
        s["configuration"], s["safety"],
    )
    return [
        f"<td>{_e(fw['version'])} ({_e(fw['target'])}, {_e(fw['build_type'])})<br>"
        f"<small>OTA {_e(fw['ota_state'])}</small></td>",
        f"<td>{_e(_uptime(s['uptime_ms']))}<br><small>reset {_e(sysm['reset_reason'])}, "
        f"time {'synced' if s['time_synced'] else 'unsynced'}"
        f"<br>device clock {_e(s['device_time_utc'])}</small></td>",
        f"<td>heap {_e(sysm['heap_free_bytes'])} (min {_e(sysm['heap_min_free_bytes'])})<br>"
        f"<small>PSRAM {_e(sysm['psram_free_bytes'])}</small></td>",
        f"<td>RSSI {_e(net['wifi_rssi_dbm'])} dBm<br><small>DNS {_e(net['dns_health'])}</small></td>",
        f"<td>MTA {_e(mta['feed_success_count'])}/8<br><small>{_e(mta['poll_state'])}, "
        f"veh {_e(mta['vehicles_resolved'])}/{_e(mta['vehicles_processed'])}, "
        f"LEDs {_e(mta['occupied_map_led_count'])}</small></td>",
        f"<td>{_e(disp['state'])}<br><small>bright {_e(disp['logical_brightness'])}, "
        f"routes {_e(_mask(disp['route_visibility_mask']))}</small></td>",
        f"<td>config {_e(cfg['load_valid'])}<br><small>HW build "
        f"{_e(saf['physical_output_build_allowed'])}, ctrl {_e(saf['controller_state'])}, "
        f"arm {_e(saf['arming_state'])}</small></td>",
    ]


def _event_row(event):
    try:
        when = datetime.fromtimestamp(int(event["received_at"]), timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")
    except (TypeError, ValueError, OverflowError, OSError):
        when = "unreadable"
    return (
        f"<tr><td>{_e(when)}</td><td>{_e(event['device_id'])}</td>"
        f"<td>{_e(event['event_type'])}</td><td>{_e(event['severity'])}</td></tr>"
    )


def render_html(devices, events, now):
    stamp = datetime.fromtimestamp(now, timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")
    body = ["<!doctype html><html lang=en><head><meta charset=utf-8>",
            "<meta name=viewport content=\"width=device-width, initial-scale=1\">",
            "<title>Brooklyn Map Monitor</title>",
            "<style>body{font:14px system-ui,sans-serif;margin:1em}.w{overflow-x:auto}"
            "table{border-collapse:collapse;width:100%}"
            "td,th{border:1px solid #ccc;padding:4px 8px;vertical-align:top;text-align:left;overflow-wrap:anywhere}"
            "small{color:#555}.b{padding:1px 6px;border-radius:3px;color:#fff;font-size:12px}"
            ".ok{background:#2a7}.warn{background:#c80}.bad{background:#c33}.dim{background:#777}</style></head><body>",
            f"<h1>Brooklyn Map Monitor</h1><p><small>Generated {stamp} (all times UTC). "
            "Application-reported telemetry, not hardware attestation.</small></p>"]
    if devices:
        body.append("<div class=w><table><tr><th>Device</th><th>State / health</th><th>Heartbeat</th><th>Firmware</th>"
                    "<th>Uptime</th><th>Memory</th><th>Network</th><th>MTA</th><th>Display</th><th>Config / safety</th></tr>")
        body.extend(_device_row(d) for d in devices)
        body.append("</table></div>")
    else:
        body.append("<p>No devices have reported yet.</p>")
    body.append("<h2>Recent events (received, UTC)</h2>")
    if events:
        body.append("<div class=w><table><tr><th>Received</th><th>Device</th><th>Event</th><th>Severity</th></tr>")
        body.extend(_event_row(e) for e in events)
        body.append("</table></div>")
    else:
        body.append("<p>No events.</p>")
    body.append("</body></html>")
    return "".join(body)


def make_handler(db_path, interval_s, allowed_hosts=("127.0.0.1", "localhost", "[::1]")):
    allowed = {host.lower() for host in allowed_hosts}

    def host_ok(header):
        # Reject unexpected Host headers (DNS-rebinding defense for a loopback service).
        header = (header or "").strip().lower()
        name = header.split("]")[0] + "]" if header.startswith("[") else header.split(":")[0]
        return name in allowed

    class Handler(BaseHTTPRequestHandler):
        def _send(self, status, content_type, data):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Content-Security-Policy", CSP)
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if not host_ok(self.headers.get("Host")):
                self._send(403, "text/plain; charset=utf-8", b"forbidden host")
                return
            path = self.path.split("?", 1)[0]
            if path not in ("/", "/api/devices"):
                self._send(404, "text/plain; charset=utf-8", b"not found")
                return
            try:
                store = Store(db_path, readonly=True)
            except (sqlite3.Error, RuntimeError, OSError):
                self._send(503, "text/plain; charset=utf-8", b"database unavailable")
                return
            try:
                now = int(time.time())
                devices = [describe_device(r, now, interval_s) for r in store.list_devices()]
                if path == "/api/devices":
                    data = json.dumps({"generated_at": now, "devices": devices}).encode()
                    self._send(200, "application/json", data)
                else:
                    page = render_html(devices, store.recent_events(limit=50), now)
                    self._send(200, "text/html; charset=utf-8", page.encode())
            except sqlite3.Error:
                self._send(503, "text/plain; charset=utf-8", b"database unavailable")
            finally:
                store.close()

        def log_message(self, *args):
            pass

    return Handler


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--db", required=True)
    parser.add_argument("--port", type=int, default=8780)
    parser.add_argument("--interval", type=int, default=DEFAULT_INTERVAL_S)
    parser.add_argument("--allowed-host", action="append", default=[],
                        help="extra Host header value accepted (for a reverse proxy that preserves Host)")
    args = parser.parse_args(argv)
    hosts = ("127.0.0.1", "localhost", "[::1]", *args.allowed_host)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(args.db, args.interval, hosts))
    print(f"Dashboard on http://127.0.0.1:{args.port}/ (loopback only)")
    server.serve_forever()


if __name__ == "__main__":
    main()
