# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Optional live MQTT subscriber for the telemetry collector.

Requires paho-mqtt (tested with 2.1.0; see tools/telemetry/requirements.txt).
The import is deferred so the rest of the package and its tests are stdlib-only.
Credentials are read from the environment only and are never logged or stored.
The collector subscribes to the three ACL-permitted topic classes and never
publishes. TLS verification is always on; port 1883 is never used.

    BROOKLYN_MQTT_COLLECTOR_USER / BROOKLYN_MQTT_COLLECTOR_PASSWORD
    python tools/telemetry/collect.py --db telemetry.sqlite3
"""

import argparse
import os
import ssl
import sys
import time
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    __package__ = "telemetry"

from .ingest import Collector  # noqa: E402
from .store import Store  # noqa: E402

DEFAULT_HOST = "mqtt.goldwein.net"
DEFAULT_PORT = 8884
SUBSCRIPTIONS = [
    ("brooklyn/v1/+/status", 1),
    ("brooklyn/v1/+/event", 1),
    ("brooklyn/v1/+/availability", 1),
]
PRUNE_INTERVAL_S = 3600


def credentials_from_env(env=os.environ):
    user = env.get("BROOKLYN_MQTT_COLLECTOR_USER")
    password = env.get("BROOKLYN_MQTT_COLLECTOR_PASSWORD")
    if not user or not password:
        raise SystemExit(
            "Set BROOKLYN_MQTT_COLLECTOR_USER and BROOKLYN_MQTT_COLLECTOR_PASSWORD "
            "in this shell (never on the command line or in files)."
        )
    return user, password


def run(args):
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        raise SystemExit("paho-mqtt is required for live collection: pip install -r tools/telemetry/requirements.txt")

    user, password = credentials_from_env()
    store = Store(args.db)
    collector = Collector(store, time.time, interval_s=args.interval)
    last_prune = [0.0]

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, protocol=mqtt.MQTTv311)
    client.username_pw_set(user, password)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)

    def on_connect(client, userdata, flags, reason_code, properties=None):
        if reason_code != 0:
            print(f"connect refused: {reason_code}", file=sys.stderr)
            return
        client.subscribe(SUBSCRIPTIONS)
        print("connected; subscribed (read-only)")

    def on_message(client, userdata, message):
        collector.handle(message.topic, message.payload, bool(message.retain))
        if time.time() - last_prune[0] > PRUNE_INTERVAL_S:
            collector.prune()
            last_prune[0] = time.time()

    client.on_connect = on_connect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=120)
    client.connect(args.host, args.port, keepalive=60)
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        pass
    finally:
        client.disconnect()
        store.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--db", required=True, help="SQLite file (keep outside Git)")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--interval", type=int, default=30, help="expected heartbeat seconds")
    args = parser.parse_args(argv)
    if args.port == 1883:
        raise SystemExit("Refusing plaintext port 1883")
    run(args)


if __name__ == "__main__":
    main()
