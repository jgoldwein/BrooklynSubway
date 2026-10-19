# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Phase 4AA hardening, failure-injection, scale, privacy and schema tests (offline)."""

import json
import logging
import random
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from telemetry import dashboard, store as store_module  # noqa: E402
from telemetry.ingest import (  # noqa: E402
    Collector, MAX_PAYLOAD_BYTES, derive_health, describe_device,
)
from telemetry.schema_validate import load_schema, validate  # noqa: E402
from telemetry.store import SCHEMA_VERSION, Store  # noqa: E402
from telemetry.test_telemetry import (  # noqa: E402
    BOOT_1, BOOT_2, DEV_A, DEV_B, T0, Clock, enc, event, status, topic,
)

CHECKS = 0
FAILURES = 0
FINDINGS = []


def check(condition, description):
    global CHECKS, FAILURES
    CHECKS += 1
    if not condition:
        FAILURES += 1
        print("FAIL:", description)


def device_uuid(index):
    return f"{index:08x}-0000-4000-8000-{index:012x}"


def new_collector(path=":memory:", clock=None):
    clock = clock or Clock()
    return Collector(Store(path), clock), clock


V1_DDL = [
    """CREATE TABLE devices_current (device_id TEXT PRIMARY KEY, device_name TEXT,
       first_seen_at INTEGER NOT NULL, boot_id TEXT, sequence INTEGER, status_json TEXT,
       status_received_at INTEGER, status_live_at INTEGER, availability TEXT,
       availability_received_at INTEGER, availability_retained INTEGER)""",
    """CREATE TABLE events (id INTEGER PRIMARY KEY AUTOINCREMENT, device_id TEXT NOT NULL,
       received_at INTEGER NOT NULL, boot_id TEXT NOT NULL, sequence INTEGER NOT NULL,
       event_type TEXT NOT NULL, severity TEXT NOT NULL, body_json TEXT NOT NULL,
       UNIQUE (device_id, boot_id, sequence))""",
    "CREATE INDEX events_device_time ON events (device_id, received_at)",
]


def test_database():
    with tempfile.TemporaryDirectory(prefix="phase4aa-db-") as tmp:
        path = Path(tmp) / "t.sqlite3"
        fresh = Store(path)
        check(fresh.schema_version() == SCHEMA_VERSION == 2, "fresh database at current schema version")
        check(fresh.db.execute("PRAGMA journal_mode").fetchone()[0] == "wal", "file database uses WAL")
        names = {r[0] for r in fresh.db.execute("SELECT name FROM sqlite_master WHERE type='index'")}
        check({"events_device_time", "events_received"} <= names, "query indexes exist")
        fresh.close()
        again = Store(path)
        check(again.schema_version() == SCHEMA_VERSION, "reopen is idempotent")
        again.close()

        old = Path(tmp) / "v1.sqlite3"
        raw = sqlite3.connect(old)
        for statement in V1_DDL:
            raw.execute(statement)
        raw.execute("INSERT INTO devices_current (device_id, first_seen_at, boot_id, sequence) VALUES (?, 1, ?, 7)", (DEV_A, BOOT_1))
        raw.execute("PRAGMA user_version = 1")
        raw.commit()
        raw.close()
        migrated = Store(old)
        check(migrated.schema_version() == SCHEMA_VERSION, "v1 database migrates to current")
        check(migrated.get_device(DEV_A)["sequence"] == 7, "migration preserves data")
        check("events_received" in {r[0] for r in migrated.db.execute("SELECT name FROM sqlite_master")}, "migration adds index")
        migrated.close()

        broken = Path(tmp) / "broken.sqlite3"
        raw = sqlite3.connect(broken)
        for statement in V1_DDL:
            raw.execute(statement)
        raw.execute("PRAGMA user_version = 1")
        raw.commit()
        raw.close()
        original = dict(store_module._MIGRATIONS)
        store_module._MIGRATIONS[2] = "CREATE INDEX ok_idx ON events (severity); CREATE INDEX bad ON missing_table (x)"
        try:
            Store(broken)
            check(False, "failing migration raises")
        except sqlite3.Error:
            check(True, "failing migration raises")
        finally:
            store_module._MIGRATIONS.clear()
            store_module._MIGRATIONS.update(original)
        raw = sqlite3.connect(broken)
        version = raw.execute("PRAGMA user_version").fetchone()[0]
        has_idx = raw.execute("SELECT count(*) FROM sqlite_master WHERE name='ok_idx'").fetchone()[0]
        raw.close()
        check(version == 1 and has_idx == 0, "failed migration rolls back atomically")

        garbage = Path(tmp) / "garbage.sqlite3"
        garbage.write_bytes(b"this is not a sqlite database" * 100)
        try:
            Store(garbage)
            check(False, "corrupt database rejected")
        except sqlite3.DatabaseError:
            check(True, "corrupt database rejected")

        ro_path = Path(tmp) / "ro.sqlite3"
        writer = Store(ro_path)
        writer.add_event(DEV_A, event(), 100)
        reader = Store(ro_path, readonly=True)
        check(len(reader.recent_events()) == 1, "read-only store reads while writer open")
        try:
            reader.db.execute("DELETE FROM events")
            check(False, "read-only store refuses writes")
        except sqlite3.OperationalError:
            check(True, "read-only store refuses writes")
        reader.close()
        writer.close()
        try:
            Store(Path(tmp) / "missing.sqlite3", readonly=True)
            check(False, "missing database refused read-only")
        except sqlite3.Error:
            check(True, "missing database refused read-only")


def test_containment_and_logging():
    collector, _ = new_collector()
    collector.store.close()
    check(collector.handle(topic(DEV_A, "status"), enc(status())) == "error:database", "database fault contained")
    collector, _ = new_collector()

    class Boom(Store):
        def save_status(self, *args, **kwargs):
            raise RuntimeError("unexpected")

    boom = Collector(Boom(), Clock())
    check(boom.handle(topic(DEV_A, "status"), enc(status())) == "error:internal", "internal fault contained")
    check(boom.handle(topic(DEV_A, "availability"), b"online") == "availability", "collector continues after a fault")

    records = []

    class Capture(logging.Handler):
        def emit(self, record):
            records.append(record.getMessage())

    handler = Capture()
    logging.getLogger().addHandler(handler)
    previous = logging.getLogger().level
    logging.getLogger().setLevel(logging.DEBUG)
    try:
        hostile = b'{"password": "hunter2-SECRET", "ssid": "HomeNet"'
        collector.handle(topic(DEV_A, "status"), hostile)
        collector.handle(topic(DEV_A, "event"), enc({**event(), "attributes": {"x": "hunter2-SECRET"}}))
        collector.handle("brooklyn/v1/hunter2-SECRET/status", b"{}")
    finally:
        logging.getLogger().removeHandler(handler)
        logging.getLogger().setLevel(previous)
    check(not any("hunter2" in r for r in records), "payload/topic contents are never logged")
    check("hunter2" not in json.dumps(collector.summary()), "summary holds only outcome categories")
    source = (ROOT / "tools/telemetry/collect.py").read_text(encoding="utf-8")
    log_lines = [line for line in source.splitlines() if "LOG." in line or "print(" in line]
    check(not any("password" in line.lower() for line in log_lines), "collector log statements never reference the password")
    check("message.payload" not in " ".join(log_lines), "collector log statements never include payloads")


def test_corrupt_rows_and_dashboard_rendering():
    check(derive_health({"display": {}}) == "UNKNOWN", "malformed status yields UNKNOWN health")
    base = {"device_id": DEV_A, "status_json": "{not json", "status_live_at": T0, "availability": "online"}
    check(describe_device(base, T0 + 1)["status"] is None, "corrupt status_json tolerated")
    check(describe_device({**base, "status_json": "[1,2]"}, T0 + 1)["status"] is None, "non-object status tolerated")

    collector, _ = new_collector()
    collector.handle(topic(DEV_A, "status"), enc(status()))
    hostile_id = "<img src=x onerror=alert(1)>"
    collector.store.db.execute(
        "INSERT INTO devices_current (device_id, device_name, first_seen_at, status_json) VALUES (?, ?, 1, ?)",
        (hostile_id, "<b>x</b>", json.dumps({"firmware": {}})))
    collector.store.db.execute(
        "INSERT INTO events (device_id, received_at, boot_id, sequence, event_type, severity, body_json) "
        "VALUES (?, 'bad-time', 'b', 1, '<script>', 'info', '{}')", (hostile_id,))
    collector.store.db.commit()
    page = dashboard.render_html(collector.snapshot(), collector.store.recent_events(), T0)
    check("<img" not in page and "<script>" not in page and "&lt;img" in page, "hostile ids/names/event types escaped")
    check("stored status unreadable" in page and "unreadable" in page, "corrupt rows render safely")
    json.dumps(collector.snapshot())
    check(True, "API serialization of odd rows works")

    check(dashboard._age(0) == "0s" and dashboard._age(119) == "119s" and dashboard._age(120) == "2m", "age formatting seconds/minutes")
    check(dashboard._age(7199) == "119m" and dashboard._age(7200) == "2h" and dashboard._age(172800) == "2d", "age formatting hours/days")
    check(dashboard._age(None) == "-", "age formatting unknown")
    check(dashboard._mask(0xFFFFFFFF) == "0xFFFFFFFF" and dashboard._mask(5) == "0x00000005" and dashboard._mask(None) == "-", "route mask rendered as hex")
    check(dashboard._uptime(90061000) == "1d 1h 1m", "uptime formatting")

    long_name = "N" * 48
    wide = Collector(Store(), Clock())
    wide.handle(topic(DEV_A, "status"), enc(status(device_name=long_name, firmware__version="v" * 32, firmware__build_id="b" * 40)))
    page = dashboard.render_html(wide.snapshot(), [], T0)
    check(long_name in page and "overflow-wrap" in page and "viewport" in page, "long values render; responsive hints present")
    check("all times UTC" in page and "device clock 2027-01-15T08:00:00Z" in page, "UTC presentation is explicit")
    check("0xFFFFFFFF" in page, "route mask shown in dashboard")

    states = Collector(Store(), Clock())
    states.handle(topic(DEV_A, "status"), enc(status()))
    states.handle(topic(DEV_A, "availability"), b"online")
    states.handle(topic(DEV_B, "availability"), b"offline")
    page = dashboard.render_html(states.snapshot(), [], T0)
    check("ONLINE" in page and "OFFLINE" in page, "online and offline devices shown together")
    states.clock.now = T0 + 90
    check(states.snapshot()[0]["state"] == "STALE", "stale state derived")
    unknown = Collector(Store(), Clock())
    unknown.store.save_availability(DEV_A, "online", T0, False)
    unknown.store.db.execute("UPDATE devices_current SET availability = NULL")
    unknown.store.db.commit()
    check(unknown.snapshot()[0]["state"] == "UNKNOWN" and "UNKNOWN" in dashboard.render_html(unknown.snapshot(), [], T0), "unknown device state")


def serve(path, hosts=("127.0.0.1", "localhost", "[::1]")):
    server = ThreadingHTTPServer(("127.0.0.1", 0), dashboard.make_handler(str(path), 30, hosts))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, f"http://127.0.0.1:{server.server_address[1]}"


def get(url, host=None):
    request = urllib.request.Request(url)
    if host is not None:
        request.add_header("Host", host)
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            return response.status, response.read(), response.headers
    except urllib.error.HTTPError as error:
        return error.code, b"", error.headers


def test_dashboard_http():
    with tempfile.TemporaryDirectory(prefix="phase4aa-http-") as tmp:
        path = Path(tmp) / "d.sqlite3"
        collector, _ = new_collector(path, Clock(int(time.time())))
        collector.handle(topic(DEV_A, "status"), enc(status()))
        server, base = serve(path)
        try:
            code, body, headers = get(base + "/")
            check(code == 200 and headers["X-Frame-Options"] == "DENY" and headers["Referrer-Policy"] == "no-referrer", "security headers present")
            check(get(base + "/", host="evil.example")[0] == 403, "foreign Host header refused")
            check(get(base + "/api/devices", host="localhost:8780")[0] == 200, "localhost Host accepted")
            check(get(base + "/", host="")[0] == 403, "empty Host refused")
            check(get(base + "/?x=<script>")[0] == 200, "query string ignored safely")
            check(get(base + "/../etc/passwd")[0] == 404, "no path traversal surface")
            check(server.server_address[0] == "127.0.0.1", "dashboard bound to loopback")
        finally:
            server.shutdown()
            server.server_close()
        proxy_server, proxy_base = serve(path, ("127.0.0.1", "monitor.goldwein.net"))
        try:
            check(get(proxy_base + "/", host="monitor.goldwein.net")[0] == 200, "explicitly allowed proxy Host accepted")
        finally:
            proxy_server.shutdown()
            proxy_server.server_close()
        collector.store.close()

        missing_server, missing_base = serve(Path(tmp) / "nope.sqlite3")
        try:
            check(get(missing_base + "/")[0] == 503, "missing database yields 503, not a crash")
            check(get(missing_base + "/")[0] == 503, "dashboard keeps serving after a database error")
        finally:
            missing_server.shutdown()
            missing_server.server_close()

        # Dashboard reads while the collector is writing.
        live_path = Path(tmp) / "live.sqlite3"
        writer, wclock = new_collector(live_path, Clock(int(time.time())))
        writer.handle(topic(DEV_A, "status"), enc(status()))
        server, base = serve(live_path)
        stop = threading.Event()
        errors = []

        def write_loop():
            local, _ = new_collector(live_path, Clock(int(time.time())))
            try:
                sequence = 2
                while not stop.is_set():
                    local.handle(topic(DEV_A, "status"), enc(status(sequence=sequence)))
                    local.handle(topic(DEV_B, "event"), enc(event(device=DEV_B, sequence=sequence)))
                    sequence += 1
            finally:
                local.store.db.close()

        thread = threading.Thread(target=write_loop)
        thread.start()
        try:
            for _ in range(40):
                code, _, _ = get(base + "/api/devices")
                if code != 200:
                    errors.append(code)
        finally:
            deadline = time.time() + 5
            while writer.store.get_device(DEV_A)["sequence"] < 5 and time.time() < deadline:
                time.sleep(0.01)
            stop.set()
            thread.join()
            server.shutdown()
            server.server_close()
        check(not errors, f"dashboard reads succeed during collector writes ({len(errors)} errors)")
        check(writer.store.get_device(DEV_A)["sequence"] >= 2, "writer made progress during reads")
        writer.store.close()


def test_failure_injection():
    rng = random.Random(4242)
    collector, _ = new_collector()
    junk = [b"", b"{", b"null", b"\x00\x01", b"[" * 5000, b'{"protocol":"brooklyn-telemetry"}', b"\xff" * 100,
            enc({**status(), "sequence": -1}), enc({**status(), "version": "1"}), b"x" * (MAX_PAYLOAD_BYTES + 1)]
    ok_devices = [device_uuid(i) for i in range(1, 6)]
    accepted = 0
    for round_ in range(400):
        payload = rng.choice(junk)
        collector.handle(topic(rng.choice(ok_devices), rng.choice(["status", "event", "availability"])), payload)
        if round_ % 20 == 0:
            device = ok_devices[(round_ // 20) % 5]
            accepted += collector.handle(topic(device, "status"), enc(status(device=device, sequence=round_))) == "status"
    check(accepted == 20, "valid devices all ingested inside a malformed storm")
    check(collector.counters.get("status", 0) == 20 and not any(k.startswith("error:") for k in collector.counters), "storm caused no internal errors")
    check(len(collector.store.list_devices()) == 5, "junk created no extra devices")

    dup, _ = new_collector()
    outcomes = [dup.handle(topic(DEV_A, "event"), enc(event())) for _ in range(500)]
    check(outcomes.count("event") == 1 and outcomes.count("duplicate") == 499 and len(dup.store.recent_events()) == 1, "duplicate storm stores one event")
    sdup = [dup.handle(topic(DEV_A, "status"), enc(status(sequence=3))) for _ in range(100)]
    check(sdup.count("status") == 1, "duplicate status storm accepted once")

    order, _ = new_collector()
    sequences = list(range(1, 60))
    rng.shuffle(sequences)
    for sequence in sequences:
        order.handle(topic(DEV_A, "status"), enc(status(sequence=sequence)))
    check(order.store.get_device(DEV_A)["sequence"] == 59, "shuffled sequences end at the maximum")
    check(json.loads(order.store.get_device(DEV_A)["status_json"])["sequence"] == 59, "stored payload matches the maximum sequence")

    with tempfile.TemporaryDirectory(prefix="phase4aa-crash-") as tmp:
        path = Path(tmp) / "crash.sqlite3"
        script = (
            "import os, sys\n"
            f"sys.path.insert(0, {str(ROOT / 'tools')!r})\n"
            "from telemetry.store import Store\n"
            f"s = Store({str(path)!r})\n"
            f"s.save_availability({DEV_A!r}, 'online', 1, False)\n"
            "s.db.execute('BEGIN')\n"
            f"s.db.execute(\"INSERT INTO devices_current (device_id, first_seen_at) VALUES ('{DEV_B}', 2)\")\n"
            "os._exit(1)\n"
        )
        subprocess.run([sys.executable, "-c", script], check=False)
        recovered = Store(path)
        check(recovered.db.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "database intact after abrupt process death")
        check(recovered.get_device(DEV_A) is not None and recovered.get_device(DEV_B) is None, "committed data kept, uncommitted discarded")
        recovered.close()

        # Restart recovery is deterministic: same inputs, same state, across reopen cycles.
        digests = []
        for _ in range(2):
            run_path = Path(tmp) / f"det{len(digests)}.sqlite3"
            det, clock = new_collector(run_path)
            for sequence in (1, 2, 2, 1, 3):
                det.handle(topic(DEV_A, "status"), enc(status(sequence=sequence)))
                det.store.close()
                det = Collector(Store(run_path), clock)
            digests.append(json.dumps(det.snapshot(), sort_keys=True))
            det.store.close()
        check(digests[0] == digests[1], "restart recovery is deterministic")


def test_scale():
    for count in (1, 10, 50, 100):
        with tempfile.TemporaryDirectory(prefix="phase4aa-scale-") as tmp:
            path = Path(tmp) / "s.sqlite3"
            collector, clock = new_collector(path, Clock(T0))
            started = time.perf_counter()
            for index in range(1, count + 1):
                device = device_uuid(index)
                collector.handle(topic(device, "availability"), b"online")
                for sequence in range(1, 6):
                    collector.handle(topic(device, "status"), enc(status(device=device, sequence=sequence)))
                    collector.handle(topic(device, "status"), enc(status(device=device, sequence=sequence)))
                for sequence in range(1, 4):
                    collector.handle(topic(device, "event"), enc(event(device=device, sequence=sequence)))
                    collector.handle(topic(device, "event"), enc(event(device=device, sequence=sequence)))
            ingest_s = time.perf_counter() - started
            snapshot = collector.snapshot()
            check(len(snapshot) == count and all(d["sequence"] == 5 and d["state"] == "ONLINE" for d in snapshot), f"{count} devices: latest state correct")
            check(len(collector.store.recent_events(limit=100000)) == 3 * count, f"{count} devices: events deduplicated")
            started = time.perf_counter()
            page = dashboard.render_html(snapshot, collector.store.recent_events(limit=50), T0)
            render_s = time.perf_counter() - started
            check(page.count("<tr>") >= count and render_s < 5, f"{count} devices: dashboard renders quickly")
            collector.store.db.commit()
            collector.store.db.execute("PRAGMA wal_checkpoint(TRUNCATE)")
            size = path.stat().st_size
            clock.now = T0 + 31 * 86400
            check(collector.prune() == 3 * count and collector.store.recent_events() == [], f"{count} devices: pruning removes only expired events")
            check(len(collector.snapshot()) == count, f"{count} devices: pruning keeps latest state")
            FINDINGS.append(f"{count:>3} devices: ingest {ingest_s:.2f}s for {count * 16} messages, render {render_s * 1000:.0f}ms, DB {size // 1024} KiB")
            collector.store.close()

    with tempfile.TemporaryDirectory(prefix="phase4aa-growth-") as tmp:
        sizes = []
        for heartbeats in (1, 200):
            path = Path(tmp) / f"g{heartbeats}.sqlite3"
            collector, _ = new_collector(path)
            for sequence in range(1, heartbeats + 1):
                collector.handle(topic(DEV_A, "status"), enc(status(sequence=sequence)))
            collector.store.db.execute("PRAGMA wal_checkpoint(TRUNCATE)")
            sizes.append(path.stat().st_size)
            collector.store.close()
        check(sizes[1] <= sizes[0] + 8192, "heartbeat history is not stored (database size does not grow with heartbeats)")


def worst_case_status():
    payload = status(device_name="N" * 48, firmware__version="v" * 32, firmware__build_id="b" * 40)
    payload["extensions"] = {f"x_{'k' * 28}{i}": "v" * 64 for i in range(8)}
    return payload


def test_privacy_schema_and_size():
    forbidden = ("ssid", "password", "passwd", "psk", "token", "csrf", "secret", "private", "verifier", "session",
                 "gateway", "dns_server", "local_ip", "ip_addr", "mac", "gtfs", "trip", "vehicle_id", "url", "history", "cookie")

    def keys(node, found):
        if isinstance(node, dict):
            for key, value in node.items():
                if key in ("properties", "$defs"):
                    for name, child in value.items():
                        found.add(name)
                        keys(child, found)
                else:
                    keys(value, found)
        elif isinstance(node, list):
            for item in node:
                keys(item, found)
        return found

    for name in ("telemetry", "event", "availability"):
        schema = load_schema(f"brooklyn-{name}-v1.schema.json")
        names = keys(schema, set())
        bad = sorted(n for n in names if any(word in n.lower() for word in forbidden))
        check(not bad, f"{name} schema has no excluded field names {bad}")
        check(schema["$id"].endswith(":v1"), f"{name} schema id carries explicit version")

    tele = load_schema("brooklyn-telemetry-v1.schema.json")
    check(tele["properties"]["version"]["const"] == 1 and tele["additionalProperties"] is False, "telemetry schema is versioned and strict")
    check(tele["properties"]["kind"]["const"] == "heartbeat", "status payload kind distinguished")
    check(load_schema("brooklyn-event-v1.schema.json")["properties"]["kind"]["const"] == "event", "event payload kind distinguished")
    check("extensions" in tele["properties"] and "extensions" not in tele["required"], "optional bounded extensions allow additive evolution")
    for section in ("integrity", "configuration", "safety"):
        check(section in tele["required"], f"{section} is required by design")

    typical = len(json.dumps(status(), separators=(",", ":")))
    worst = len(json.dumps(worst_case_status(), separators=(",", ":")))
    check(validate(worst_case_status(), tele) == [], "worst-case heartbeat is schema-valid")
    check(typical <= 2048, f"typical heartbeat <= 2 KiB ({typical} B)")
    check(worst <= MAX_PAYLOAD_BYTES, f"worst-case heartbeat within the 4 KiB ceiling ({worst} B)")
    FINDINGS.append(f"payload sizes: typical {typical} B, schema worst case {worst} B (ceiling {MAX_PAYLOAD_BYTES})")
    collector, _ = new_collector()
    check(collector.handle(topic(DEV_A, "status"), json.dumps(worst_case_status()).encode()) == "status", "worst-case heartbeat ingested")

    collector.handle(topic(DEV_A, "event"), enc(event()))
    dump = json.dumps(collector.store.list_devices() + collector.store.recent_events()).lower()
    page = dashboard.render_html(collector.snapshot(), collector.store.recent_events(), T0).lower()
    for word in ("ssid", "password", "token", "csrf", "secret", "gateway", "trip_id", "vehicle_id", "gtfs"):
        check(word not in dump, f"stored data excludes '{word}'")
        check(word not in page, f"dashboard excludes '{word}'")

    check(collector.handle(topic(DEV_A, "status"), enc({**status(sequence=99), "wifi_ssid": "Home"})) == "rejected:schema", "unexpected top-level field (e.g. SSID) rejected")
    network = status(sequence=100)
    network["network"]["local_ip"] = "192.168.1.5"
    check(collector.handle(topic(DEV_A, "status"), enc(network)) == "rejected:schema", "unexpected network field (e.g. IP) rejected")
    for source_name in ("ingest.py", "store.py", "dashboard.py", "collect.py"):
        text = (ROOT / "tools/telemetry" / source_name).read_text(encoding="utf-8").lower()
        check("setinsecure" not in text and "cert_none" not in text and "tls_insecure" not in text, f"{source_name} never disables TLS verification")
    check("1883" not in (ROOT / "tools/telemetry/collect.py").read_text(encoding="utf-8").replace("Refusing plaintext port 1883", "").replace("== 1883", "").replace("port 1883 is never used", ""),
          "no plaintext broker port used")


def main():
    for test in (test_database, test_containment_and_logging, test_corrupt_rows_and_dashboard_rendering,
                 test_dashboard_http, test_failure_injection, test_scale, test_privacy_schema_and_size):
        test()
    for line in FINDINGS:
        print("finding:", line)
    print(f"Telemetry hardening tests: {CHECKS} checks, {FAILURES} failures")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
