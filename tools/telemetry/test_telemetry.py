#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Offline tests for the Phase 4AA collector, store, and dashboard."""

import copy
import json
import sys
import tempfile
import threading
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from telemetry import collect, dashboard  # noqa: E402
from telemetry.ingest import (  # noqa: E402
    Collector, MAX_PAYLOAD_BYTES, derive_health, derive_state,
)
from telemetry.schema_validate import load_schema, validate  # noqa: E402
from telemetry.store import SCHEMA_VERSION, Store  # noqa: E402

CHECKS = 0
FAILURES = 0

DEV_A = "11111111-1111-4111-8111-111111111111"
DEV_B = "22222222-2222-4222-9222-222222222222"
BOOT_1 = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
BOOT_2 = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"
T0 = 1_800_000_000


def check(condition, description):
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print("FAIL:", description)


class Clock:
    def __init__(self, now=T0):
        self.now = now

    def __call__(self):
        return self.now


def status(device=DEV_A, boot=BOOT_1, sequence=1, **overrides):
    payload = {
        "protocol": "brooklyn-telemetry", "version": 1, "kind": "heartbeat",
        "device_id": device, "device_name": "Test Map", "boot_id": boot,
        "sequence": sequence, "uptime_ms": 3_600_000, "time_synced": True,
        "device_time_utc": "2027-01-15T08:00:00Z",
        "firmware": {"target": "brooklyn-s3", "version": "1.0.0", "build_id": "abc123",
                     "build_type": "production", "image_sha256": "0" * 64,
                     "active_partition": "app0", "ota_state": "confirmed"},
        "system": {"reset_reason": "power_on", "boot_count": 3, "heap_free_bytes": 150000,
                   "heap_min_free_bytes": 100000, "psram_free_bytes": 6000000,
                   "psram_min_free_bytes": 5900000},
        "network": {"wifi_connected": True, "wifi_rssi_dbm": -55, "dns_health": "ok"},
        "mta": {"feed_success_count": 8, "feed_attempted_count": 8, "feed_success_mask": 255,
                "poll_state": "complete", "last_complete_age_s": 12, "vehicles_processed": 100,
                "vehicles_resolved": 95, "occupied_map_led_count": 90, "collision_count": 0},
        "display": {"state": "Fresh", "logical_brightness": 128, "route_visibility_mask": 4294967295},
        "integrity": {"application_reported_status": "reported_ok", "semantic_check": "pass",
                      "led_mapping_check": "pass"},
        "configuration": {"schema_version": 1, "load_valid": True},
        "safety": {"physical_output_build_allowed": False, "controller_state": "not_present",
                   "arming_state": "not_present", "current_limit_state": "not_present"},
    }
    for dotted, value in overrides.items():
        target = payload
        parts = dotted.split("__")
        for part in parts[:-1]:
            target = target[part]
        target[parts[-1]] = value
    return payload


def event(device=DEV_A, boot=BOOT_1, sequence=1, event_type="boot", severity="info"):
    return {
        "protocol": "brooklyn-telemetry", "version": 1, "kind": "event", "device_id": device,
        "boot_id": boot, "sequence": sequence, "uptime_ms": 1000, "time_synced": False,
        "device_time_utc": None,         "event_type": event_type, "severity": severity,
                "attributes": {"reset_reason": "power_on"} if event_type == "boot" else {},
            }


def enc(document):
    return json.dumps(document).encode()


def topic(device, kind):
    return f"brooklyn/v1/{device}/{kind}"


def new_collector(path=":memory:", clock=None):
    clock = clock or Clock()
    return Collector(Store(path), clock), clock


def test_validator():
    schema = load_schema("brooklyn-telemetry-v1.schema.json")
    check(validate(status(), schema) == [], "valid heartbeat validates")
    check(validate(event(), load_schema("brooklyn-event-v1.schema.json")) == [], "valid event validates")
    bad = status()
    bad["extra"] = 1
    check(validate(bad, schema), "unknown top-level property rejected")
    bad = status(display__logical_brightness=True)
    check(validate(bad, schema), "boolean is not an integer")
    bad = status(display__logical_brightness=256)
    check(validate(bad, schema), "range maximum enforced")
    check(validate(status(time_synced=False), schema), "time_synced=false requires null device time")
    check(validate(status(system__psram_free_bytes=None), schema) == [], "nullable psram accepted")
    check(validate(status(device_id="not-a-uuid"), schema), "device_id pattern enforced")
    check(validate(status(device_name="x" * 49), schema), "device_name length limit")
    check(validate(status(extensions={"bad key": 1}), schema), "extension key pattern enforced")
    check(validate(status(extensions={"x_ok": 1}), schema) == [], "bounded extension accepted")
    missing = status()
    del missing["safety"]
    check(validate(missing, schema), "missing required section rejected")
    avail = load_schema("brooklyn-availability-v1.schema.json")
    check(validate("online", avail) == [] and validate("up", avail), "availability enum enforced")


def test_ingest_basics():
    collector, clock = new_collector()
    check(collector.handle(topic(DEV_A, "status"), enc(status())) == "status", "status accepted")
    row = collector.store.get_device(DEV_A)
    check(row["boot_id"] == BOOT_1 and row["sequence"] == 1, "status stored")
    check(collector.handle(topic(DEV_A, "status"), enc(status())) == "duplicate_or_old", "duplicate status ignored")
    check(collector.handle(topic(DEV_A, "status"), enc(status(sequence=0))) == "duplicate_or_old", "older sequence ignored")
    check(collector.handle(topic(DEV_A, "status"), enc(status(sequence=2))) == "status", "newer sequence accepted")
    check(collector.handle(topic(DEV_A, "status"), enc(status(boot=BOOT_2, sequence=0))) == "status", "reboot (new boot_id) accepted")
    check(collector.store.get_device(DEV_A)["boot_id"] == BOOT_2, "reboot updates boot_id")
    check(collector.handle(topic(DEV_A, "event"), enc(event())) == "event", "event accepted")
    check(collector.handle(topic(DEV_A, "event"), enc(event())) == "duplicate", "duplicate event ignored (QoS1 redelivery)")
    check(collector.handle(topic(DEV_A, "event"), enc(event(boot=BOOT_2))) == "event", "same sequence in new boot is distinct")
    check(len(collector.store.recent_events()) == 2, "two distinct events stored")
    check(collector.handle(topic(DEV_A, "availability"), b"online") == "availability", "availability accepted")
    check(collector.handle(topic(DEV_B, "status"), enc(status(device=DEV_B))) == "status", "second device accepted")
    check(len(collector.snapshot()) == 2, "multiple devices tracked")
    check(collector.store.get_device(DEV_B)["sequence"] == 1, "devices are independent")


def test_rejections():
    collector, _ = new_collector()
    cases = [
        ("malformed json", topic(DEV_A, "status"), b"{not json", "rejected:malformed_json"),
        ("invalid utf-8", topic(DEV_A, "status"), b"\xff\xfe", "rejected:malformed_json"),
        ("NaN constant", topic(DEV_A, "status"), b'{"a": NaN}', "rejected:malformed_json"),
        ("array payload", topic(DEV_A, "status"), b"[1]", "rejected:schema"),
        ("oversized", topic(DEV_A, "status"), b" " * (MAX_PAYLOAD_BYTES + 1), "rejected:oversized"),
        ("unknown schema version", topic(DEV_A, "status"), enc({**status(), "version": 2}), "rejected:unknown_schema"),
        ("unknown protocol", topic(DEV_A, "status"), enc({**status(), "protocol": "other"}), "rejected:unknown_schema"),
        ("wrong type", topic(DEV_A, "status"), enc(status(sequence="1")), "rejected:schema"),
        ("wrong kind", topic(DEV_A, "status"), enc({**status(), "kind": "event"}), "rejected:schema"),
        ("topic/device mismatch", topic(DEV_B, "status"), enc(status()), "rejected:device_id_mismatch"),
        ("event device mismatch", topic(DEV_B, "event"), enc(event()), "rejected:device_id_mismatch"),
        ("bad topic id", "brooklyn/v1/not-a-uuid/status", enc(status()), "rejected:bad_topic"),
        ("foreign topic", "other/v1/x/status", b"{}", "rejected:bad_topic"),
        ("extra topic level", topic(DEV_A, "status") + "/x", b"{}", "rejected:bad_topic"),
        ("bad availability", topic(DEV_A, "availability"), b"maybe", "rejected:bad_availability"),
        ("json-string availability", topic(DEV_A, "availability"), b'"online"', "rejected:bad_availability"),
        ("non-bytes payload", topic(DEV_A, "status"), "text", "rejected:bad_payload_type"),
    ]
    for name, t, payload, expected in cases:
        check(collector.handle(t, payload) == expected, f"reject: {name}")
    check(collector.handle(topic(DEV_A, "event"), enc(event()), retained=True) == "rejected:retained_event", "retained events refused")
    missing = status()
    del missing["mta"]
    check(collector.handle(topic(DEV_A, "status"), enc(missing)) == "rejected:schema", "missing field rejected")
    check(collector.store.list_devices() == [], "rejected messages create no device rows")
    check(collector.handle(topic(DEV_A, "event"), enc({**event(), "event_type": "nope"})) == "rejected:schema", "unknown event type rejected")


def test_state_derivation():
    interval = 30
    base = {"status_json": "{}", "status_live_at": T0, "availability": "online"}
    check(derive_state(base, T0 + 75, interval)[0] == "ONLINE", "ONLINE at 2i+15 boundary")
    check(derive_state(base, T0 + 76, interval)[0] == "STALE", "STALE just beyond 2i+15")
    check(derive_state(base, T0 + 120, interval)[0] == "STALE", "STALE at 3i+30 boundary")
    check(derive_state(base, T0 + 121, interval)[0] == "OFFLINE", "OFFLINE beyond 3i+30")
    check(derive_state({**base, "availability": "offline"}, T0 + 1, interval)[0] == "OFFLINE", "availability offline wins")
    check(derive_state({**base, "availability": None}, T0 + 1, interval)[0] == "UNKNOWN", "no availability -> UNKNOWN")
    check(derive_state({"availability": None}, T0, interval)[0] == "UNKNOWN", "empty device UNKNOWN")
    retained_only = {"status_json": "{}", "status_live_at": None, "availability": "online"}
    check(derive_state(retained_only, T0, interval)[0] == "STALE", "retained-only status never reads ONLINE")
    check(derive_state(base, T0 - 5, interval)[1] == 0, "negative age clamps to zero")

    collector, clock = new_collector()
    collector.handle(topic(DEV_A, "availability"), b"online")
    collector.handle(topic(DEV_A, "status"), enc(status()))
    check(collector.snapshot()[0]["state"] == "ONLINE", "live heartbeat + online -> ONLINE")
    clock.now += 200
    check(collector.snapshot()[0]["state"] == "OFFLINE", "silence -> OFFLINE")
    collector.handle(topic(DEV_A, "availability"), b"offline", retained=True)
    check(collector.snapshot()[0]["availability_retained"], "retained flag recorded")


def test_health():
    check(derive_health(status()) == "HEALTHY", "nominal heartbeat is HEALTHY")
    check(derive_health(None) == "UNKNOWN", "no status -> UNKNOWN health")
    check(derive_health(status(display__state="StaleHold")) == "WARNING", "stale display warns")
    check(derive_health(status(system__reset_reason="panic")) == "WARNING", "panic reset warns")
    check(derive_health(status(configuration__load_valid=False)) == "FAULT", "invalid config is FAULT")
    check(derive_health(status(firmware__ota_state="rollback")) == "FAULT", "OTA rollback is FAULT")
    check(derive_health(status(safety__current_limit_state="over_limit")) == "FAULT", "over-limit is FAULT")
    check(derive_health(status(integrity__led_mapping_check="fail")) == "FAULT", "mapping failure is FAULT")


def test_retained_and_restart():
    with tempfile.TemporaryDirectory(prefix="phase4aa-tests-") as tmp:
        path = Path(tmp) / "t.sqlite3"
        collector, clock = new_collector(path)
        collector.handle(topic(DEV_A, "status"), enc(status(sequence=5)))
        collector.handle(topic(DEV_A, "event"), enc(event(sequence=9)))
        collector.store.close()

        clock.now += 40
        reopened = Collector(Store(path), clock)
        check(reopened.store.schema_version() == SCHEMA_VERSION, "schema version persisted")
        check(reopened.handle(topic(DEV_A, "event"), enc(event(sequence=9))) == "duplicate", "event dedup survives restart")
        check(reopened.handle(topic(DEV_A, "status"), enc(status(sequence=5)), retained=True) == "duplicate_or_old",
              "retained redelivery of known sequence ignored after restart")
        old_boot = status(boot=BOOT_2, sequence=1, device_time_utc="2027-01-15T07:00:00Z")
        check(reopened.handle(topic(DEV_A, "status"), enc(old_boot), retained=True) == "stale_retained",
              "older retained message from a different boot is rejected")
        unsynced = status(boot=BOOT_2, sequence=1, time_synced=False, device_time_utc=None)
        check(reopened.handle(topic(DEV_A, "status"), enc(unsynced), retained=True) == "stale_retained",
              "unprovable retained boot change is rejected")
        newer = status(boot=BOOT_2, sequence=1, device_time_utc="2027-01-15T09:00:00Z")
        check(reopened.handle(topic(DEV_A, "status"), enc(newer), retained=True) == "status_retained",
              "newer retained message from a new boot is accepted")
        row = reopened.store.get_device(DEV_A)
        check(row["status_live_at"] == T0, "retained delivery does not refresh live heartbeat time")
        reopened.store.close()

        fresh_path = Path(tmp) / "fresh.sqlite3"
        fresh, _ = new_collector(fresh_path)
        fresh.handle(topic(DEV_B, "status"), enc(status(device=DEV_B)), retained=True)
        fresh.handle(topic(DEV_B, "availability"), b"online", retained=True)
        snap = fresh.snapshot()[0]
        check(snap["state"] == "STALE" and snap["status_is_retained_only"], "first-seen retained status is STALE, flagged")
        fresh.store.close()

        newer_db = Path(tmp) / "future.sqlite3"
        future = Store(newer_db)
        future.db.execute(f"PRAGMA user_version = {SCHEMA_VERSION + 1}")
        future.close()
        try:
            Store(newer_db)
            check(False, "newer schema refused")
        except RuntimeError:
            check(True, "newer schema refused")


def test_retention_and_deletion():
    collector, clock = new_collector()
    collector.handle(topic(DEV_A, "event"), enc(event(sequence=1)))
    clock.now += 29 * 86400
    collector.handle(topic(DEV_A, "event"), enc(event(sequence=2)))
    check(collector.prune() == 0, "events inside 30 days kept")
    clock.now += 2 * 86400
    check(collector.prune() == 1, "event older than 30 days pruned")
    check(len(collector.store.recent_events()) == 1, "newer event survives pruning")
    collector.store.delete_device(DEV_A)
    check(collector.store.list_devices() == [] and collector.store.recent_events() == [], "device deletion removes data")


def test_dashboard():
    now = T0
    empty = dashboard.render_html([], [], now)
    check("No devices have reported yet." in empty and "No events." in empty, "empty dashboard renders")

    collector, clock = new_collector()
    hostile = status(device_name="<script>alert(1)</script>&\"'")
    collector.handle(topic(DEV_A, "availability"), b"online")
    collector.handle(topic(DEV_A, "status"), enc(hostile))
    collector.handle(topic(DEV_A, "event"), enc(event(event_type="boot")))
    collector.handle(topic(DEV_B, "availability"), b"offline")
    page = dashboard.render_html(collector.snapshot(), collector.store.recent_events(), now)
    check("<script>" not in page and "&lt;script&gt;" in page, "device name is HTML-escaped")
    for needle in ("1.0.0", "MTA 8/8", "RSSI -55", "heap 150000", "Fresh", "ONLINE", "HEALTHY", "boot"):
        check(needle in page, f"dashboard shows {needle}")
    check("no heartbeat received" in page, "device without heartbeat shown")

    with tempfile.TemporaryDirectory(prefix="phase4aa-dash-") as tmp:
        path = Path(tmp) / "d.sqlite3"
        live = Collector(Store(path), Clock(int(__import__("time").time())))
        live.handle(topic(DEV_A, "status"), enc(status()))
        live.store.close()
        server = ThreadingHTTPServer(("127.0.0.1", 0), dashboard.make_handler(str(path), 30))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base = f"http://127.0.0.1:{server.server_address[1]}"
            with urllib.request.urlopen(base + "/") as response:
                body = response.read().decode()
                check(response.status == 200 and "Test Map" in body, "HTTP dashboard serves page")
                check("default-src 'none'" in response.headers["Content-Security-Policy"], "CSP header set")
            with urllib.request.urlopen(base + "/api/devices") as response:
                data = json.loads(response.read())
                check(data["devices"][0]["device_id"] == DEV_A, "JSON API lists device")
            try:
                urllib.request.urlopen(base + "/nope")
                check(False, "unknown path 404")
            except urllib.error.HTTPError as error:
                check(error.code == 404, "unknown path 404")
            request = urllib.request.Request(base + "/", method="POST", data=b"x")
            try:
                urllib.request.urlopen(request)
                check(False, "POST not allowed")
            except urllib.error.HTTPError as error:
                check(error.code == 501, "POST not supported (read-only)")
        finally:
            server.shutdown()
            server.server_close()


def test_security():
    try:
        collect.credentials_from_env({})
        check(False, "missing credentials stop collector")
    except SystemExit as exit_info:
        message = str(exit_info)
        check("BROOKLYN_MQTT_COLLECTOR_USER" in message, "missing credentials stop collector")
    secret = "s3cret-value-xyz"
    user, password = collect.credentials_from_env(
        {"BROOKLYN_MQTT_COLLECTOR_USER": "u", "BROOKLYN_MQTT_COLLECTOR_PASSWORD": secret}
    )
    check(password == secret, "credentials come from environment")
    try:
        collect.main(["--db", "x.sqlite3", "--port", "1883"])
        check(False, "plaintext port refused")
    except SystemExit as exit_info:
        check("1883" in str(exit_info), "plaintext port refused")
    check(collect.SUBSCRIPTIONS == [("brooklyn/v1/+/status", 1), ("brooklyn/v1/+/event", 1),
                                    ("brooklyn/v1/+/availability", 1)], "collector subscribes only to the ACL-permitted topics")
    source = (ROOT / "tools/telemetry/collect.py").read_text(encoding="utf-8")
    check(".publish(" not in source, "collector never publishes")

    collector, _ = new_collector()
    collector.handle(topic(DEV_A, "status"), enc(status()))
    dump = json.dumps(collector.store.list_devices())
    check(secret not in dump, "no credential in stored data")
    for forbidden in ("password", "ssid", "token"):
        check(forbidden not in dump.lower(), f"stored data contains no '{forbidden}' field")


def main():
    for test in (test_validator, test_ingest_basics, test_rejections, test_state_derivation,
                 test_health, test_retained_and_restart, test_retention_and_deletion,
                 test_dashboard, test_security):
        test()
    print(f"Telemetry tests: {CHECKS} checks, {FAILURES} failures")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
