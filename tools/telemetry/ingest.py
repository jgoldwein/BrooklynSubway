# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""Message validation, de-duplication, and state derivation for telemetry.

The collector is transport-agnostic: feed it (topic, payload_bytes, retained)
tuples from a live MQTT subscriber or from synthetic tests. It never publishes.
"""

import json
import re
from datetime import datetime

from .schema_validate import load_schema, validate

TOPIC_RE = re.compile(r"^brooklyn/v1/([^/+#]+)/(status|event|availability)$")
DEVICE_ID_RE = re.compile(
    r"^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
)
MAX_PAYLOAD_BYTES = 4096
DEFAULT_INTERVAL_S = 30
EVENT_RETENTION_DAYS = 30

ONLINE, STALE, OFFLINE, UNKNOWN = "ONLINE", "STALE", "OFFLINE", "UNKNOWN"
HEALTHY, WARNING, FAULT = "HEALTHY", "WARNING", "FAULT"

LOW_HEAP_WARNING_BYTES = 20000


class _Reject(Exception):
    pass


def _no_constant(name):
    raise ValueError(f"non-finite constant {name}")


def _parse_json(payload):
    try:
        text = payload.decode("utf-8")
        return json.loads(text, parse_constant=_no_constant)
    except (UnicodeDecodeError, ValueError, RecursionError):
        raise _Reject("malformed_json")


def _parse_time(value):
    if not value:
        return None
    return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()


class Collector:
    def __init__(self, store, clock, interval_s=DEFAULT_INTERVAL_S, topic_aliases=None):
        self.store = store
        self.clock = clock
        self.interval_s = interval_s
        # Test-only: maps a non-UUID topic segment (a temporary broker test
        # account namespace) to the UUID expected in the payload.
        self.topic_aliases = dict(topic_aliases or {})
        self.telemetry_schema = load_schema("brooklyn-telemetry-v1.schema.json")
        self.event_schema = load_schema("brooklyn-event-v1.schema.json")
        self.counters = {}

    def _count(self, key):
        self.counters[key] = self.counters.get(key, 0) + 1

    def handle(self, topic, payload, retained=False):
        """Process one message; return a short outcome string (never raises)."""
        try:
            outcome = self._handle(topic, payload, retained)
        except _Reject as reject:
            outcome = f"rejected:{reject}"
        self._count(outcome)
        return outcome

    def _handle(self, topic, payload, retained):
        match = TOPIC_RE.match(topic if isinstance(topic, str) else "")
        if not match:
            raise _Reject("bad_topic")
        segment, kind = match.groups()
        device_id = self.topic_aliases.get(segment, segment)
        if not DEVICE_ID_RE.match(device_id):
            raise _Reject("bad_topic")
        if not isinstance(payload, (bytes, bytearray)):
            raise _Reject("bad_payload_type")
        if len(payload) > MAX_PAYLOAD_BYTES:
            raise _Reject("oversized")
        now = int(self.clock())

        if kind == "availability":
            if bytes(payload) not in (b"online", b"offline"):
                raise _Reject("bad_availability")
            self.store.save_availability(device_id, bytes(payload).decode(), now, retained)
            return "availability"

        document = _parse_json(payload)
        if not isinstance(document, dict):
            raise _Reject("schema")
        if document.get("protocol") != "brooklyn-telemetry" or document.get("version") != 1:
            raise _Reject("unknown_schema")
        if kind == "status":
            return self._status(device_id, document, now, retained)
        return self._event(device_id, document, now, retained)

    def _status(self, device_id, document, now, retained):
        if document.get("kind") != "heartbeat":
            raise _Reject("schema")
        if validate(document, self.telemetry_schema):
            raise _Reject("schema")
        if document["device_id"] != device_id:
            raise _Reject("device_id_mismatch")
        current = self.store.get_device(device_id)
        if current and current["boot_id"]:
            if current["boot_id"] == document["boot_id"]:
                if document["sequence"] <= current["sequence"]:
                    return "duplicate_or_old"
            elif not self._boot_change_allowed(current, document, retained):
                return "stale_retained" if retained else "stale_boot"
        self.store.save_status(device_id, document, now, live=not retained)
        return "status_retained" if retained else "status"

    @staticmethod
    def _boot_change_allowed(current, document, retained):
        """Cross-boot ordering uses the validated device clock.

        A different boot_id is accepted only if its device time is strictly later
        than the stored one; equal or earlier is stale (equal is ambiguous, so the
        existing state wins). If either time is unusable no chronology is invented:
        a live message is accepted (a real reboot may precede SNTP), a retained
        message is rejected.
        """
        try:
            stored = _parse_time(json.loads(current["status_json"])["device_time_utc"])
            incoming = _parse_time(document["device_time_utc"])
        except (TypeError, ValueError, KeyError):
            stored = incoming = None
        if stored is None or incoming is None:
            return not retained
        return incoming > stored

    def _event(self, device_id, document, now, retained):
        if document.get("kind") != "event":
            raise _Reject("schema")
        if retained:
            raise _Reject("retained_event")
        if validate(document, self.event_schema):
            raise _Reject("schema")
        if document["device_id"] != device_id:
            raise _Reject("device_id_mismatch")
        return "event" if self.store.add_event(device_id, document, now) else "duplicate"

    def prune(self):
        return self.store.prune_events(int(self.clock()), EVENT_RETENTION_DAYS)

    def snapshot(self):
        """Derived view of every known device."""
        now = int(self.clock())
        return [describe_device(row, now, self.interval_s) for row in self.store.list_devices()]


def derive_state(row, now, interval_s=DEFAULT_INTERVAL_S):
    """Return (state, status_age_s). Age is measured from the last LIVE heartbeat."""
    live_at = row.get("status_live_at")
    age = None if live_at is None else max(0, now - live_at)
    availability = row.get("availability")
    if availability == "offline":
        return OFFLINE, age
    if age is None:
        if row.get("status_json") and availability == "online":
            return STALE, None
        return UNKNOWN, None
    if age > 3 * interval_s + 30:
        return OFFLINE, age
    if availability == "online" and age <= 2 * interval_s + 15:
        return ONLINE, age
    if availability is None:
        return UNKNOWN, age
    return STALE, age


def derive_health(status):
    """Application-reported severity, independent of reachability."""
    if not status:
        return UNKNOWN
    fault = (
        status["configuration"]["load_valid"] is False
        or status["firmware"]["ota_state"] == "rollback"
        or status["safety"]["controller_state"] == "fault"
        or status["safety"]["arming_state"] == "fault"
        or status["safety"]["current_limit_state"] == "over_limit"
        or status["integrity"]["semantic_check"] == "fail"
        or status["integrity"]["led_mapping_check"] == "fail"
    )
    if fault:
        return FAULT
    mta = status["mta"]
    warning = (
        status["display"]["state"] not in ("Fresh", "WaitingForData")
        or status["firmware"]["ota_state"] == "pending_verification"
        or status["integrity"]["application_reported_status"] == "reported_degraded"
        or status["network"]["dns_health"] == "failed"
        or not status["network"]["wifi_connected"]
        or status["system"]["heap_min_free_bytes"] < LOW_HEAP_WARNING_BYTES
        or status["system"]["reset_reason"] in ("panic", "watchdog", "brownout")
        or mta["feed_success_count"] < mta["feed_attempted_count"]
        or status["configuration"]["load_valid"] is None
    )
    return WARNING if warning else HEALTHY


def describe_device(row, now, interval_s=DEFAULT_INTERVAL_S):
    state, age = derive_state(row, now, interval_s)
    status = json.loads(row["status_json"]) if row.get("status_json") else None
    return {
        "device_id": row["device_id"],
        "device_name": row.get("device_name"),
        "state": state,
        "health": derive_health(status),
        "status_age_s": age,
        "availability": row.get("availability"),
        "availability_retained": bool(row.get("availability_retained")),
        "status_is_retained_only": row.get("status_live_at") is None and status is not None,
        "boot_id": row.get("boot_id"),
        "sequence": row.get("sequence"),
        "status": status,
    }
