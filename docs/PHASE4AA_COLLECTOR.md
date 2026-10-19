# Phase 4AA host-side collector, store, and dashboard

Host-only tooling for the MQTT telemetry design in
[PHASE4AA_REMOTE_TELEMETRY_DESIGN.md](PHASE4AA_REMOTE_TELEMETRY_DESIGN.md).
No firmware, broker, or hardware is touched by anything here.

## Components (`tools/telemetry/`)

| File | Role |
| --- | --- |
| `schema_validate.py` | Stdlib validator for the JSON-Schema subset used by `docs/schemas/`. Fails at load time if a schema uses an unsupported keyword. |
| `store.py` | SQLite (`PRAGMA user_version` migrations). `devices_current` (latest heartbeat + availability) and `events` (unique `(device_id, boot_id, sequence)`). No heartbeat history. |
| `ingest.py` | Transport-agnostic `Collector.handle(topic, payload, retained)`: size cap (4096 B), strict JSON, schema validation, topic/`device_id` match, dedup, ordering, state/health derivation. |
| `collect.py` | Optional live subscriber (paho-mqtt 2.1.0, `requirements.txt`). Credentials only from `BROOKLYN_MQTT_COLLECTOR_USER/PASSWORD`. Subscribes read-only to the three topic classes; never publishes; refuses port 1883. |
| `dashboard.py` | Loopback-only (127.0.0.1) read-only HTML + `/api/devices` JSON, escaped output, CSP. |

Run (credentials set only in the current shell, never on a command line or file):

```powershell
pip install -r tools/telemetry/requirements.txt
python tools/telemetry/collect.py --db C:\path\outside\git\telemetry.sqlite3
python tools/telemetry/dashboard.py --db C:\path\outside\git\telemetry.sqlite3
python tools/run_telemetry_tests.py
```

## Ingestion rules

- Same `boot_id`: sequence must strictly increase, otherwise duplicate/old and ignored (QoS 1 redelivery).
- New `boot_id` on a live message: accepted (reboot).
- Retained status with a different `boot_id` than stored: accepted only if both
  `device_time_utc` values are present and the new one is later; otherwise
  `stale_retained`. A live heartbeat will follow within one interval.
- Retained status never refreshes the "last live heartbeat" time, so a
  collector restart cannot make a dead device look ONLINE.
- Events: retained events are refused; duplicates dropped by the unique key.
- Messages whose payload `device_id` differs from the topic are dropped.
- Rejected payloads are counted by reason only; their contents are never stored or logged.
- Events are pruned after 30 days; `delete_device` removes a device and its events.
- Known limitation: a delayed live message from an older boot arriving after a
  newer boot would be accepted (indistinguishable without a trusted clock).

## State model

Reachability (`ONLINE`/`STALE`/`OFFLINE`/`UNKNOWN`) uses the design thresholds
with the expected interval (default 30 s, `--interval`): ONLINE = availability
`online` and live heartbeat age <= 2i+15 s; STALE (design term "late") beyond
that; OFFLINE = availability `offline` or age > 3i+30 s; UNKNOWN = no usable
evidence. Health (`HEALTHY`/`WARNING`/`FAULT`/`UNKNOWN`) is separate and
application-reported only (not hardware attestation).

## Deployment checklist (documentation only; nothing deployed)

- Record the Mosquitto version on the broker (currently unrecorded).
- Run `collect.py` as a systemd service under an unprivileged user with an
  `EnvironmentFile` (mode 0600) holding only the collector credentials.
- Keep the SQLite file outside any web root and Git; back up consistently with the 30-day event window.
- A public dashboard (for example `https://monitor.goldwein.net/`) must be a
  separate Apache reverse proxy with authentication in front of `dashboard.py`;
  the auth model is undecided. Never bind the dashboard beyond loopback without it.
- Never use the `goldwein` account for the collector or devices.

## Production provisioning (deferred, undecided)

Goal: one unique MQTT identity per map (`bkmap-<id>`), publish-only ACL on its
own three topics. Options to decide later: factory-provisioned per-unit
credentials; enrollment over the Wi-Fi setup AP using a one-time token; a
claim-code flow against a provisioning service. Risk: credentials stored in
ESP32 NVS are extractable with physical access (flash encryption/NVS
encryption mitigate but are not enabled by this design). A leaked per-device
credential is limited by ACL to publishing that device's own topics, and must
be revocable individually. A shared password across devices is not acceptable
in production.

## Embedded MQTT library comparison (design only; no dependency added)

| Candidate | Notes |
| --- | --- |
| ESP-IDF `esp-mqtt` | Native to ESP-IDF/Arduino-ESP32 (no extra dependency), TLS via mbedTLS with CA bundle or pinned cert, MQTT 3.1.1, LWT, QoS 1, auto-reconnect with backoff, runs in its own task. Needs care for task/memory budget and for the TLS-after-SNTP rule. |
| PubSubClient | Small, popular, synchronous, MQTT 3.1.1; works over `WiFiClientSecure`. Default 256 B packet buffer (must be raised for ~2 KiB payloads), QoS 1 publish is not supported (publish is QoS 0 only), which does not meet the QoS 1 requirement. |
| ArduinoMqttClient | Clean API over any `Client`; supports QoS 0-2, will, keepalive. Blocking polling model; reconnect logic is the caller's. |
| Others (AsyncMqttClient, etc.) | Async, but less maintained and heavier lock-in; not recommended for a safety-conscious firmware. |

Recommendation: prefer `esp-mqtt` (already part of the framework, QoS 1 + LWT +
backoff built in); `ArduinoMqttClient` is the fallback. Confirm the exact
framework-bundled version and heap/PSRAM cost on the XIAO bench target before
adopting. Verify these claims against current library docs at that time.
