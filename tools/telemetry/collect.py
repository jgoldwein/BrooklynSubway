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
import logging
import os
import signal
import sqlite3
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
SUMMARY_INTERVAL_S = 300
LOG = logging.getLogger("brooklyn.collector")


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
    try:
        store = Store(args.db)
    except (sqlite3.Error, RuntimeError) as error:
        raise SystemExit(f"cannot open database: {type(error).__name__}")
    collector = Collector(store, time.time, interval_s=args.interval, topic_aliases=args.aliases)
    last_prune = [0.0]
    last_summary = [time.time()]

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, protocol=mqtt.MQTTv311)
    client.username_pw_set(user, password)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)

    def on_connect(client, userdata, flags, reason_code, properties=None):
        if reason_code != 0:
            LOG.error("connect refused: %s", reason_code)
            return
        client.subscribe(SUBSCRIPTIONS)
        LOG.info("connected to %s:%s; subscribed (read-only)", args.host, args.port)

    def on_disconnect(client, userdata, flags, reason_code, properties=None):
        LOG.warning("disconnected (%s); automatic reconnect with backoff", reason_code)

    def on_message(client, userdata, message):
        collector.handle(message.topic, message.payload, bool(message.retain))
        now = time.time()
        if now - last_prune[0] > PRUNE_INTERVAL_S:
            last_prune[0] = now
            try:
                collector.prune()
            except sqlite3.Error as error:
                LOG.error("event pruning failed: %s", type(error).__name__)
        if now - last_summary[0] > SUMMARY_INTERVAL_S:
            last_summary[0] = now
            LOG.info("ingest summary: %s", collector.summary())

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=120)

    def stop(signum, frame):
        client.disconnect()

    signal.signal(signal.SIGINT, stop)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, stop)
    # connect_async + retry_first_connection keeps retrying if the broker is down at start.
    client.connect_async(args.host, args.port, keepalive=60)
    try:
        client.loop_forever(retry_first_connection=True)
    finally:
        LOG.info("shutting down; final summary: %s", collector.summary())
        store.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--db", required=True, help="SQLite file (keep outside Git)")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--interval", type=int, default=30, help="expected heartbeat seconds")
    parser.add_argument("--test-alias", action="append", default=[], metavar="SEGMENT=UUID",
                        help="test only: map a temporary account topic segment to a device UUID")
    args = parser.parse_args(argv)
    args.aliases = dict(item.split("=", 1) for item in args.test_alias)
    if args.port == 1883:
        raise SystemExit("Refusing plaintext port 1883")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    run(args)


if __name__ == "__main__":
    main()
