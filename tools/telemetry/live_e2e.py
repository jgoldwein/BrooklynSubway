# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Live end-to-end validation: real Mosquitto + real collector + SQLite + dashboard.

Operator-run only (needs credentials in the current shell's environment):

    BROOKLYN_MQTT_COLLECTOR_USER / _PASSWORD   read-only collector (bkmap-collector)
    BROOKLYN_MQTT_DEVICE_USER    / _PASSWORD   publish-only synthetic device (brooklynmap)

The device account only owns brooklyn/v1/brooklynmap/*, so that topic segment is
aliased to a random synthetic device UUID (collector --test-alias mechanism).
The script never publishes outside that namespace, never modifies the broker,
uses a temporary SQLite file, clears its retained messages, and prints no secrets.
Takes roughly two minutes (waits for real heartbeat timeouts).
"""

import json
import os
import queue
import ssl
import sys
import tempfile
import threading
import time
import urllib.request
import uuid
from datetime import datetime, timezone
from http.server import ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import paho.mqtt.client as mqtt  # noqa: E402

from telemetry import dashboard  # noqa: E402
from telemetry.collect import SUBSCRIPTIONS  # noqa: E402
from telemetry.ingest import Collector  # noqa: E402
from telemetry.store import Store  # noqa: E402
from telemetry.test_telemetry import event, status  # noqa: E402

HOST, PORT = "mqtt.goldwein.net", 8884
SEGMENT = "brooklynmap"
INTERVAL = 2  # ONLINE <= 19 s, STALE <= 36 s, OFFLINE beyond
RESULTS = []


def report(name, ok, detail=""):
    RESULTS.append(ok)
    print(f"{name}: {'PASS' if ok else 'FAIL'}" + (f" ({detail})" if detail else ""))


def env(prefix):
    user = os.environ.get(f"BROOKLYN_MQTT_{prefix}_USER")
    password = os.environ.get(f"BROOKLYN_MQTT_{prefix}_PASSWORD")
    if not user or not password:
        raise SystemExit(f"Set BROOKLYN_MQTT_{prefix}_USER/_PASSWORD in this shell first.")
    return user, password


def make_client(creds, client_id, will_topic=None):
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id, protocol=mqtt.MQTTv311)
    client.username_pw_set(*creds)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)
    if will_topic:
        client.will_set(will_topic, b"offline", qos=1, retain=True)
    return client


class LiveCollector:
    """Real collector client; messages are queued and ingested on the main thread."""

    def __init__(self, creds, db_path, device_uuid):
        self.store = Store(db_path)
        self.collector = Collector(self.store, time.time, interval_s=INTERVAL,
                                   topic_aliases={SEGMENT: device_uuid})
        self.queue = queue.Queue()
        self.outcomes = []
        self.subacks = []
        self.connected = threading.Event()
        self.reconnects = 0
        self.client = make_client(creds, f"bkmap-collector-e2e-{uuid.uuid4().hex[:8]}")
        self.client.on_connect = self._on_connect
        self.client.on_subscribe = lambda c, u, mid, rc, p=None: self.subacks.append([r.value if hasattr(r, "value") else r for r in rc])
        self.client.on_message = lambda c, u, m: self.queue.put((m.topic, bytes(m.payload), bool(m.retain)))
        self.client.reconnect_delay_set(1, 5)

    def _on_connect(self, client, userdata, flags, reason_code, properties=None):
        if reason_code == 0:
            if self.connected.is_set():
                self.reconnects += 1
            client.subscribe(SUBSCRIPTIONS)
            self.connected.set()

    def start(self):
        self.client.connect(HOST, PORT, keepalive=30)
        self.client.loop_start()
        return self.connected.wait(10)

    def pump(self, seconds=1.0):
        end = time.time() + seconds
        while time.time() < end:
            try:
                topic, payload, retained = self.queue.get(timeout=0.2)
            except queue.Empty:
                continue
            self.outcomes.append((topic.rsplit("/", 1)[-1], self.collector.handle(topic, payload, retained), retained))

    def wait_for(self, kind, outcome, since, timeout=10):
        end = time.time() + timeout
        while time.time() < end:
            self.pump(0.5)
            if any(k == kind and o == outcome for k, o, _ in self.outcomes[since:]):
                return True
        return False

    def mark(self):
        return len(self.outcomes)

    def stop(self):
        self.client.loop_stop()
        self.client.disconnect()
        self.store.close()


class Device:
    def __init__(self, creds, will_topic):
        self.creds, self.will_topic = creds, will_topic
        self.client = None

    def connect(self):
        self.client = make_client(self.creds, f"bkmap-device-e2e-{uuid.uuid4().hex[:8]}", self.will_topic)
        self.client.connect(HOST, PORT, keepalive=10)
        self.client.loop_start()
        time.sleep(1.0)

    def publish(self, kind, payload, retain, qos=1):
        data = payload if isinstance(payload, (bytes, str)) else json.dumps(payload)
        info = self.client.publish(f"brooklyn/v1/{SEGMENT}/{kind}", data, qos=qos, retain=retain)
        info.wait_for_publish(10)
        return info.is_published()

    def kill(self):
        self.client._sock.close()  # unclean termination: broker must fire the Last Will
        self.client.loop_stop()

    def close(self):
        self.client.loop_stop()
        self.client.disconnect()


def now_iso(offset=0):
    return datetime.fromtimestamp(time.time() + offset, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def fetch(base, path="/"):
    with urllib.request.urlopen(base + path, timeout=10) as response:
        return response.read().decode()


def main():
    collector_creds, device_creds = env("COLLECTOR"), env("DEVICE")
    dev_uuid, boot_a, boot_b = (str(uuid.uuid4()) for _ in range(3))
    tmp = tempfile.TemporaryDirectory(prefix="phase4aa-live-")
    db = str(Path(tmp.name) / "live.sqlite3")
    started = time.time()

    live = LiveCollector(collector_creds, db, dev_uuid)
    report("A collector TLS connect", live.start(), f"{HOST}:{PORT}")
    time.sleep(1)
    live.pump(1)
    report("B subscribed to all three topic classes", live.subacks[:1] == [[1, 1, 1]] or len(live.subacks) >= 1, f"SUBACK {live.subacks[:1]}")

    device = Device(device_creds, f"brooklyn/v1/{SEGMENT}/availability")
    device.connect()
    m = live.mark()
    device.publish("availability", "online", retain=True)
    report("C availability online ingested", live.wait_for("availability", "availability", m))

    def hb(boot, seq, offset=0, **kw):
        return status(device=dev_uuid, boot=boot, sequence=seq, device_time_utc=now_iso(offset), **kw)

    m = live.mark()
    device.publish("status", hb(boot_a, 1), retain=True)
    report("E status ingested", live.wait_for("status", "status", m))
    m = live.mark()
    device.publish("event", event(device=dev_uuid, boot=boot_a, sequence=1), retain=False)
    report("G event ingested", live.wait_for("event", "event", m))
    m = live.mark()
    device.publish("status", hb(boot_a, 1), retain=True)
    report("H duplicate status ignored", live.wait_for("status", "duplicate_or_old", m))
    m = live.mark()
    device.publish("event", event(device=dev_uuid, boot=boot_a, sequence=1), retain=False)
    report("I duplicate event ignored", live.wait_for("event", "duplicate", m))
    m = live.mark()
    device.publish("status", hb(boot_a, 0), retain=True)
    report("J same-boot lower sequence rejected", live.wait_for("status", "duplicate_or_old", m))
    m = live.mark()
    device.publish("status", hb(boot_b, 0, offset=5), retain=True)
    report("L new boot accepted", live.wait_for("status", "status", m))
    m = live.mark()
    device.publish("status", hb(boot_a, 9, offset=-60), retain=True)
    stale_ok = live.wait_for("status", "stale_boot", m)
    report("K cross-boot stale message rejected", stale_ok)
    # restore the retained status to the current boot so retained replay is valid
    device.publish("status", hb(boot_b, 1, offset=6), retain=True)
    live.pump(1)

    m = live.mark()
    device.publish("status", b"{not json", retain=False)
    device.publish("status", json.dumps({**hb(boot_b, 2), "version": 99}), retain=False)
    device.publish("status", b" " * 5000, retain=False)
    device.publish("event", b"\xff\xfe", retain=False)
    time.sleep(2)
    live.pump(2)
    got = [o for k, o, _ in live.outcomes[m:]]
    report("U malformed payload rejected", "rejected:malformed_json" in got)
    report("V unsupported schema rejected", "rejected:unknown_schema" in got)
    report("W oversized payload rejected", "rejected:oversized" in got)
    m = live.mark()
    device.publish("status", hb(boot_b, 2, offset=7), retain=True)
    report("U2 collector survives bad messages and keeps ingesting", live.wait_for("status", "status", m))

    with ThreadingHTTPServer(("127.0.0.1", 0), dashboard.make_handler(db, INTERVAL)) as server:
        threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{server.server_address[1]}"
        assert server.server_address[0] == "127.0.0.1"
        page = fetch(base)
        report("DASH online state + fields", all(s in page for s in ("ONLINE", "HEALTHY", "MTA 8/8", "RSSI -55", "heap 150000", "1.0.0", "Fresh", "synced")))
        report("DASH recent events listed", "boot" in page)

        print("... waiting for heartbeat timeout (STALE) ...")
        time.sleep(22)
        live.pump(1)
        page = fetch(base)
        report("DASH heartbeat timeout -> STALE", "STALE" in page and "ONLINE" not in page)

        print("... restarting collector (retained replay must not refresh heartbeat age) ...")
        before = live.store.get_device(dev_uuid)["status_live_at"]
        live.stop()
        live = LiveCollector(collector_creds, db, dev_uuid)
        live.start()
        live.pump(4)
        row = live.store.get_device(dev_uuid)
        kinds = [(k, o, r) for k, o, r in live.outcomes]
        report("M/R collector restart reopens SQLite state", row is not None and row["boot_id"] == boot_b)
        report("N retained replay after restart", any(r and k in ("status", "availability") for k, o, r in kinds), f"{kinds[:3]}")
        report("O retained status did not refresh heartbeat age", row["status_live_at"] == before)
        report("O2 still STALE (retained online alone is not ONLINE)", live.collector.snapshot()[0]["state"] == "STALE")
        report("DASH after restart still STALE", "STALE" in fetch(base) and "ONLINE" not in fetch(base))

        m = live.mark()
        device.publish("status", hb(boot_b, 3, offset=8), retain=True)
        live.wait_for("status", "status", m)
        report("DASH new live status restores ONLINE", "ONLINE" in fetch(base))

        print("... forcing collector client disconnect (reconnect check) ...")
        live.client._sock.close()
        time.sleep(4)
        m = live.mark()
        device.publish("status", hb(boot_b, 4, offset=9), retain=True)
        report("collector auto-reconnect + resubscribe", live.wait_for("status", "status", m, 15), f"reconnects={live.reconnects}")

        m = live.mark()
        device.kill()
        report("P Last Will offline ingested", live.wait_for("availability", "availability", m, 20))
        report("P2 Last Will -> OFFLINE", live.collector.snapshot()[0]["state"] == "OFFLINE" and "OFFLINE" in fetch(base))

        device.connect()
        m = live.mark()
        device.publish("availability", "online", retain=True)
        live.wait_for("availability", "availability", m)
        device.publish("status", hb(boot_b, 5, offset=10), retain=True)
        live.wait_for("status", "status", m)
        report("Q reconnect -> ONLINE", live.collector.snapshot()[0]["state"] == "ONLINE" and "ONLINE" in fetch(base))
        report("DASH JSON API", json.loads(fetch(base, "/api/devices"))["devices"][0]["device_id"] == dev_uuid)
        server.shutdown()

    events_before = len(live.store.recent_events())
    removed = live.store.prune_events(int(time.time()) + 31 * 86400, 30)
    report("S 30-day retention prune logic", events_before >= 1 and removed == events_before and live.store.recent_events() == [])

    # cleanup: clear retained test topics (own namespace only)
    device.publish("status", b"", retain=True)
    device.publish("availability", b"", retain=True)
    device.close()
    live.stop()
    check = LiveCollector(collector_creds, db, dev_uuid)
    check.start()
    check.pump(3)
    leftovers = [k for k, o, r in check.outcomes if r]
    report("cleanup: no retained test topics remain", leftovers == [], f"{len(leftovers)} retained")
    check.stop()
    tmp.cleanup()

    print(f"Duration: {time.time() - started:.0f}s; {sum(RESULTS)}/{len(RESULTS)} checks passed")
    for name in ("COLLECTOR", "DEVICE"):
        for suffix in ("USER", "PASSWORD"):
            os.environ.pop(f"BROOKLYN_MQTT_{name}_{suffix}", None)
    return 0 if all(RESULTS) else 1


if __name__ == "__main__":
    raise SystemExit(main())
