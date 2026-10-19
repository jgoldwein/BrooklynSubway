# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

"""SQLite persistence for the Phase 4AA telemetry collector.

Only the latest heartbeat per device and a bounded event log are stored.
Heartbeat history is deliberately not kept. No credentials are stored.
"""

import json
import sqlite3
from pathlib import Path


def _split_statements(script):
    return [part.strip() for part in script.split(";") if part.strip()]

SCHEMA_VERSION = 2
PRUNE_BATCH = 500
BUSY_TIMEOUT_S = 10

_MIGRATIONS = {
    2: """
    CREATE INDEX events_received ON events (received_at);
    """,
    1: """
    CREATE TABLE devices_current (
        device_id TEXT PRIMARY KEY,
        device_name TEXT,
        first_seen_at INTEGER NOT NULL,
        boot_id TEXT,
        sequence INTEGER,
        status_json TEXT,
        status_received_at INTEGER,
        status_live_at INTEGER,
        availability TEXT,
        availability_received_at INTEGER,
        availability_retained INTEGER
    );
    CREATE TABLE events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        device_id TEXT NOT NULL,
        received_at INTEGER NOT NULL,
        boot_id TEXT NOT NULL,
        sequence INTEGER NOT NULL,
        event_type TEXT NOT NULL,
        severity TEXT NOT NULL,
        body_json TEXT NOT NULL,
        UNIQUE (device_id, boot_id, sequence)
    );
    CREATE INDEX events_device_time ON events (device_id, received_at);
    """,
}


class Store:
    def __init__(self, path=":memory:", readonly=False):
        self.readonly = readonly
        if readonly:
            uri = Path(str(path)).resolve().as_uri() + "?mode=ro"
            self.db = sqlite3.connect(uri, uri=True, timeout=BUSY_TIMEOUT_S)
        else:
            self.db = sqlite3.connect(str(path), timeout=BUSY_TIMEOUT_S)
        self.db.row_factory = sqlite3.Row
        try:
            if not readonly:
                self.db.execute("PRAGMA journal_mode = WAL")
                self.db.execute("PRAGMA synchronous = NORMAL")
                self._migrate()
            elif self.schema_version() != SCHEMA_VERSION:
                raise RuntimeError("database schema does not match this dashboard version")
        except Exception:
            self.db.close()
            raise

    def close(self):
        self.db.close()

    def _migrate(self):
        version = self.db.execute("PRAGMA user_version").fetchone()[0]
        if version > SCHEMA_VERSION:
            raise RuntimeError(f"database schema {version} is newer than supported {SCHEMA_VERSION}")
        for target in range(version + 1, SCHEMA_VERSION + 1):
            # Apply each step atomically: DDL and the version bump commit together.
            self.db.execute("BEGIN IMMEDIATE")
            try:
                for statement in _split_statements(_MIGRATIONS[target]):
                    self.db.execute(statement)
                self.db.execute(f"PRAGMA user_version = {target}")
                self.db.execute("COMMIT")
            except Exception:
                self.db.execute("ROLLBACK")
                raise

    def schema_version(self):
        return self.db.execute("PRAGMA user_version").fetchone()[0]

    def get_device(self, device_id):
        row = self.db.execute(
            "SELECT * FROM devices_current WHERE device_id = ?", (device_id,)
        ).fetchone()
        return dict(row) if row else None

    def _ensure(self, device_id, now):
        self.db.execute(
            "INSERT OR IGNORE INTO devices_current (device_id, first_seen_at) VALUES (?, ?)",
            (device_id, now),
        )

    def save_status(self, device_id, payload, now, live):
        with self.db:
            self._ensure(device_id, now)
            self.db.execute(
                """UPDATE devices_current SET device_name = ?, boot_id = ?, sequence = ?,
                   status_json = ?, status_received_at = ?,
                   status_live_at = CASE WHEN ? THEN ? ELSE status_live_at END
                   WHERE device_id = ?""",
                (payload["device_name"], payload["boot_id"], payload["sequence"],
                 json.dumps(payload, sort_keys=True, separators=(",", ":")), now,
                 1 if live else 0, now, device_id),
            )

    def save_availability(self, device_id, state, now, retained):
        with self.db:
            self._ensure(device_id, now)
            self.db.execute(
                """UPDATE devices_current SET availability = ?, availability_received_at = ?,
                   availability_retained = ? WHERE device_id = ?""",
                (state, now, 1 if retained else 0, device_id),
            )

    def add_event(self, device_id, payload, now):
        """Insert an event; return False if it duplicates (device, boot, sequence)."""
        with self.db:
            self._ensure(device_id, now)
            cursor = self.db.execute(
                """INSERT OR IGNORE INTO events
                   (device_id, received_at, boot_id, sequence, event_type, severity, body_json)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (device_id, now, payload["boot_id"], payload["sequence"],
                 payload["event_type"], payload["severity"],
                 json.dumps(payload, sort_keys=True, separators=(",", ":"))),
            )
        return cursor.rowcount == 1

    def prune_events(self, now, retention_days):
        """Delete expired events in small batches so ingestion is never blocked long."""
        cutoff = now - retention_days * 86400
        total = 0
        while True:
            with self.db:
                cursor = self.db.execute(
                    "DELETE FROM events WHERE id IN "
                    "(SELECT id FROM events WHERE received_at < ? LIMIT ?)",
                    (cutoff, PRUNE_BATCH),
                )
            total += cursor.rowcount
            if cursor.rowcount < PRUNE_BATCH:
                return total

    def list_devices(self):
        rows = self.db.execute("SELECT * FROM devices_current ORDER BY device_id").fetchall()
        return [dict(row) for row in rows]

    def recent_events(self, device_id=None, limit=50):
        query = "SELECT * FROM events"
        args = []
        if device_id is not None:
            query += " WHERE device_id = ?"
            args.append(device_id)
        query += " ORDER BY received_at DESC, id DESC LIMIT ?"
        args.append(limit)
        return [dict(row) for row in self.db.execute(query, args).fetchall()]

    def delete_device(self, device_id):
        with self.db:
            self.db.execute("DELETE FROM events WHERE device_id = ?", (device_id,))
            self.db.execute("DELETE FROM devices_current WHERE device_id = ?", (device_id,))
