# Phase 4AA — Opt-In Remote Operational Telemetry Design

**Status: DESIGN COMPLETE FOR REVIEW / IMPLEMENTATION NOT STARTED.**

This architecture, protocol, and privacy proposal does not authorize firmware,
server, dashboard, broker, deployment, automatic-update, or hardware
implementation. The referenced Mosquitto broker is **UNVERIFIED**. No DNS,
port, TLS, authentication, or version checks have been run as part of this
design revision.

## 1. Baseline and principles

Design baseline: `main` at
`13c1cf86ccaf462c56f6afc8e39dbc1c15817c7c`, following Phase 4Z implementation
`dea08f2ecfadb4fe06f66051b09f7391ecb4d459`, tag
`phase-4z-unknown-stop-audit`.

The production network build baseline is 93,308 B RAM / 1,116,665 B flash.
The production target is ESP32-S3-WROOM-1U-N16R8, 16 MB flash / 8 MB OPI PSRAM.
Phase 4W provides signed ECDSA P-256/SHA-256 OTA with target binding; Phase 4V
provides local administrator authentication and CSRF protection.
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains required.

Remote Monitoring remains:

- OFF by default; explicit administrator/user opt-in and disable at any time.
- Device-initiated outbound connection only; no inbound ESP32 connection,
  device-site port forwarding, VPN, or Tailscale requirement.
- Monitoring-only: no remote administration, reboot, configuration, Wi-Fi,
  brightness, routes, LED commands, OTA commands, or firmware installation.
- Encrypted in transit, privacy-minimized, and bounded in CPU/RAM/network use.
- Lower priority than hardware safety, display operation, MTA processing,
  local configuration, and OTA safety.
- Separate from automatic-update checking/installation.

## 2. Recommended system architecture

```text
Brooklyn Map (MQTT publisher only)
    -> mqtt.goldwein.net:8884 (Mosquitto, TLS)
    -> local read-only subscriber / collector
    -> SQLite
    -> authenticated dashboard at https://monitor.goldwein.net/
```

The Mosquitto broker and dashboard may run on one Linux host. The collector
may connect locally or over a private interface, but its broker connection
must still use an authenticated account and the selected TLS policy. Apache
serves or reverse-proxies the human dashboard; it is not the telemetry
transport or a device enrollment endpoint.

**Broker verification status (2026-10-19).** Verified from a Windows host
against `mqtt.goldwein.net:8884` (resolved 98.114.181.200): DNS, TCP, and TLS
1.3 (`TLS_AES_256_GCM_SHA384`) pass; the certificate is a Let's Encrypt
certificate for `mqtt.goldwein.net` valid 2026-10-10 through 2027-01-08 and
passes Python default-context chain and hostname verification. MQTT 3.1.1
passes: anonymous CONNECT is rejected (`Not authorized`), authenticated
CONNECT passes, QoS 1 publish/subscribe passes with exactly one delivery,
retained messages are delivered to a fresh subscriber, and a retained Last Will
`offline` is published after an unclean client termination. Test retained
topics were cleared. MQTT 5 was not tested; MQTT 3.1.1 is the target protocol.

**Device ACL verification (2026-10-19).** Classic `acl_file`
(`/etc/mosquitto/brooklyn-8884.acl`, per Joel) on the 8884 listener. With a
temporary publish-only account (`brooklynmap`, write-only on its own three
topics) and a separate observer: own status/event/availability publishes are
delivered, retained availability and the retained Last Will `offline` work,
publishes to another device namespace and to an unrelated topic are **not
delivered**, and the device receives **no live or retained messages** for its
own, another, or an unrelated namespace. Note: Mosquitto grants the SUBACK
(`[1]`) for a device account even though nothing is ever delivered, so a SUBACK
alone must never be treated as proof of authorization; verify by delivery.
Before this ACL was installed the same account was unrestricted. The read-only
collector (`bkmap-collector`) was also verified: it connects, subscribes to the
three `brooklyn/v1/+/{status,event,availability}` filters, receives live and
retained device publications, receives nothing outside those classes even when
subscribed to `#`, `brooklyn/#`, or an unrelated filter (SUBACK granted but no
delivery), and none of its publishes (device topics, another device, unrelated)
are delivered to an unrestricted observer. **Broker classification: VERIFIED
READY FOR PHASE 4AA TELEMETRY.** Still pending: Mosquitto version and the
per-device production scheme (`brooklynmap` is a temporary test identity).
Production firmware must connect only to
`mqtt.goldwein.net:8884` with certificate verification. No fallback to
plaintext port 1883 is allowed. A temporary port 1883 probe is only a
separately authorized infrastructure diagnostic, never a device test or
production configuration.

Tailscale is not required. Home Assistant is optional future integration
only, not part of the protocol or initial implementation.

## 3. Transport decision and TLS/time policy

### MQTT over TLS

Use MQTT over TLS at `mqtt.goldwein.net:8884`. Port 1883 is not a production
telemetry transport. The client must validate the broker certificate chain
and hostname against a trusted CA policy. Do not use plaintext fallback,
`setInsecure()`, skipped certificate checks, or insecure reconnect behavior.
Do not embed credentials in topics or client IDs.

MQTT is selected over the earlier HTTPS POST proposal because Joel prefers
Mosquitto and the product needs retained current state, persistent connection
management, broker Last Will availability, and a local subscriber fan-out.
MQTT requires a client library, persistent TLS/socket resources, topic/ACL
policy, and careful keepalive scheduling; this decision should be revisited
only if implementation evidence shows those costs cannot meet the bounded
resource and MTA-priority requirements.

### SNTP gate

Trustworthy time is a prerequisite for opening the MQTT/TLS connection and
validating certificate dates. Before SNTP is valid, telemetry remains pending
and makes no broker connection attempt. Normal map operation, MTA polling,
display, and local UI continue. After valid SNTP, the MQTT client may connect
when the scheduler grants time/resources. Do not weaken TLS validation to
report before time sync.

### MQTT version and library

Use MQTT 3.1.1 unless a later version is justified by the selected ESP32
library and broker. Select a client with bounded/non-blocking service,
QoS 1 publishing, retained publication, Last Will, keepalive, TLS certificate
validation, and no need for inbound subscriptions. The current repository does
not include an MQTT dependency. Evaluate the framework's available `esp-mqtt`
support versus a maintained Arduino-compatible library before implementation;
document any dependency/version in `platformio.ini` and README as required.
Do not select a QoS-0-only publisher merely to avoid a dependency.

## 4. Topic and message design

The topic namespace is versioned and uses `bkmap-<UUID>` (the canonical
lowercase hyphenated device UUID with a fixed `bkmap-` prefix) as the topic
segment:

```text
brooklyn/v1/bkmap-<UUID>/status
brooklyn/v1/bkmap-<UUID>/event
brooklyn/v1/bkmap-<UUID>/availability
```

Example topic shape only:
`brooklyn/v1/bkmap-123e4567-e89b-42d3-a456-426614174000/status`.
This is not a real device identity. No `command`, `control`, `reboot`, `ota`,
`firmware`, or `configuration` topic exists. There is no device subscription
namespace.

| Topic | Payload | QoS | Retained | Semantics |
|---|---|---:|---:|---|
| `.../status` | Versioned heartbeat JSON | 1 | Yes | Latest complete status only; overwrite retained value each heartbeat/reconnect |
| `.../event` | Versioned event JSON | 1 | No | Important transitions only; collector deduplicates QoS 1 redelivery |
| `.../availability` | UTF-8 `online` or `offline` | 1 | Yes | Current broker-visible connection state; offline is also configured as LWT |

QoS 1 is at-least-once, so delivery can duplicate. Status and events include
`boot_id` and monotonically increasing `sequence`; collector deduplicates by
`(device_id, boot_id, sequence)`. Do not rely on exactly-once delivery.
Status has only one retained message per device; routine heartbeat history is
not retained in the broker.

The detailed topic contract is in
[`PHASE4AA_MQTT_TOPIC_SCHEMA.md`](PHASE4AA_MQTT_TOPIC_SCHEMA.md). Payload
schemas are under `docs/schemas/`.

## 5. Connection, availability, and broker semantics

### Connect and publish ordering

1. Require remote monitoring enabled, valid persisted device identity and
   credentials, Wi-Fi connectivity, trustworthy SNTP time, and available
   scheduler/heap budget.
2. Configure client TLS trust and hostname verification for
   `mqtt.goldwein.net:8884`; configure a retained QoS 1 Last Will on the exact
   device `.../availability` topic with payload `offline`.
3. Connect using stable client ID equal to the device topic ID and its
   per-device broker username/password. The broker permits this identity only
   to publish its three own topics and disallows subscriptions.
4. After successful CONNACK, publish retained QoS 1 `online` to availability;
   wait for PUBACK within the bounded publish deadline.
5. Publish the current retained QoS 1 status snapshot; wait for PUBACK.
6. Publish queued non-retained QoS 1 events in sequence order within bounded
   service time. Drop/coalesce stale ordinary events if the queue is full.
7. Publish future heartbeats to retained status and significant transitions
   to non-retained event while keeping the connection alive.
8. On intentional disable/shutdown, publish retained QoS 1 `offline`, wait
   briefly for PUBACK, then send a clean MQTT DISCONNECT. If acknowledgement
   fails or the connection drops first, the broker/session state and later
   collector heartbeat-age logic govern display; do not claim offline
   publication succeeded.

The Last Will is installed during CONNECT before any `online` publication.
Mosquitto publishes the retained `offline` Will if the connection ends
unexpectedly. A clean MQTT DISCONNECT suppresses the Will, so the client must
publish `offline` itself before disconnecting. If the device loses power,
Wi-Fi, or broker reachability, the Will is delivered when the broker detects
the ungraceful loss (bounded by keepalive/transport timeout, not instantaneous).

### Keepalive and cadence

Keep MQTT connected across heartbeats; do not reconnect for every report.
Heartbeat choices are 15, 30, 60, or 300 seconds, with 30 seconds as the
recommended enabled default. Recommend MQTT keepalive 60 seconds: the client
must send protocol traffic/PINGREQ often enough to stay below it. The broker
normally detects a silent peer after approximately 1.5 × keepalive, subject
to network/TCP failure detection and broker scheduling. Do not confuse
keepalive with heartbeat cadence.

### Dashboard connectivity state

The collector records broker receipt time for every status and event and
consumes retained availability. It must never declare a device online merely
because a retained status exists.

- **OFFLINE:** retained/current availability is `offline`, or no status has
  arrived within `3 × configured_interval + 30 seconds`.
- **ONLINE:** availability is `online` and the latest accepted status age is
  at most `2 × configured_interval + 15 seconds`.
- **LATE:** not OFFLINE, but missing/freshness threshold for ONLINE is exceeded
  (including retained `online` with stale status).

`HEALTHY`, `WARNING`, and `FAULT` are a separate device-health severity. A
device cannot report its own offline state.

### Broker restart and stale retained state

After broker restart, connected clients reconnect with capped backoff, install
the Will again, publish retained `online`, and immediately republish the
latest current retained status before draining events. Mosquitto persistence
should preserve retained messages and sessions as configured, but a retained
`online` or old status is never sufficient to mark a device online: the
collector uses its own receipt timestamp and heartbeat-age rule. During
collector restart, it restores retained status/availability from Mosquitto,
and inspects the MQTT RETAIN flag to distinguish retained replay from live
publication. It marks data stale based on broker message timestamp only if
trustworthy and otherwise uses collector receive time from SQLite; do not
reset age merely because the collector resubscribed. MQTT 3.1.1 retained
messages do not carry original publish time, so persist collector receipt time
in SQLite across collector restart.

On collector restart, a newly received retained status is not a new heartbeat.
If SQLite has an earlier receive time for the same `(device, boot, sequence)`,
keep the earlier time and state. If there is no prior receipt record, mark the
retained snapshot's age unknown/stale until a fresh status publication arrives;
do not treat subscription replay as evidence that the device is currently
online.

## 6. Device authentication, identity, and ACL

### Recommended first implementation

Use a unique MQTT username and strong random per-device password over
verified TLS, with strict Mosquitto ACLs. Keep an independent stable random
device UUID for the payload/topic identity. Derive the MQTT username and
client ID from the UUID's 32 lowercase hex digits (no hyphens), so ACL `%u`
can constrain publications to that identity. The display name is presentation
metadata only and never authentication identity.

Generate a 256-bit random password on the provisioning side; show it only
during enrollment, never log or reuse it. Broker stores a salted password
verifier using Mosquitto's supported password-file mechanism, not plaintext
password. Because a device must reconnect, the device stores its password in
its own monitoring NVS namespace. Physical flash access may expose it; without
Flash Encryption/Secure Boot, do not claim hardware-rooted credential secrecy.

### Alternatives considered

| Option | Benefits | Costs / assessment |
|---|---|---|
| Unique MQTT username + random password over TLS | Native Mosquitto password file and ACL integration; simple enrollment/revocation/rotation; no payload signing; suitable first version | Secret stored on device; TLS and broker are trust boundary; extraction enables impersonation until revoked |
| Mutual TLS unique client certificate | Strong transport-layer client identity; no MQTT password; broker can validate client cert | Per-device CA, issuance, renewal, revocation/CRL, ESP certificate/key storage and Mosquitto client-cert configuration add substantial lifecycle complexity |
| Broker credentials plus P-256 payload signatures | End-to-end message authenticity beyond broker, useful across untrusted brokers/forwarders | Additional key generation/storage/signature implementation and server verification, payload/key lifecycle; does not prevent extraction/cloning; little benefit for a trusted self-hosted broker that delivers to its own collector |

For this first self-hosted broker design, per-device password + TLS + ACL is
recommended. Per-device cryptographic identity remains the stable UUID and
unique broker credential. Do **not** require application-level ECDSA
signatures in v1. Reconsider payload signing only if there is a documented
need to distrust the broker or prove origin after data leaves it.

### Publish-only structure and ACL

The ESP32 MQTT client has no subscribe calls, no subscription topic filters,
and no operational-message callback or command dispatcher. Set clean session
and do not register an inbound command callback. Broker ACL grants only
`write` on:

```text
brooklyn/v1/%u/status
brooklyn/v1/%u/event
brooklyn/v1/%u/availability
```

All other publish/read permissions are absent (deny by default). Set
`use_username_as_clientid true` where supported, requiring the MQTT Client ID
to match the authenticated username; otherwise validate the client ID in the
broker policy and enforce that it equals the compact device ID. A device
password for username A cannot publish to username B's topic because `%u`
expands to A. Never use broad wildcard publish grants for device identities.

The device account has **no SUBSCRIBE/read permission** for any topic, not even
inside its own namespace. This publish-only property is intentional and is the
no-remote-command security model: the broker must answer any SUBSCRIBE from a
device account with a failure code, and the device must never receive messages.
In MQTT 3.1.1 a denied publish still receives PUBACK, so publish isolation must
be verified with a separate authorized observer that confirms the message is
not delivered. The
collector has a separate identity with read access only to
`brooklyn/v1/+/status`, `.../event`, and `.../availability`; it has no device
publish permission. Broker administration uses a separate offline/admin
credential and is not present on the ESP32 or dashboard.

### MQTT client identity

**Naming policy.** Username, MQTT client ID, and the topic device segment are
all the same string, so `%u` ACL substitution confines each credential to its
own topics. For production this is the stable 32-character lowercase hex device
ID and must match `^[0-9a-f]{32}$`; the provisioning tool rejects any other
value, which also rules out `+`, `#`, `/`, and `$` in a substituted username.
Disposable test identities use the explicit form `bkmap-test-NNN` with explicit
per-user ACL blocks (never `%u` patterns) and must be clearly named TEST. The
display name is separate and is never used for authentication. The collector
uses a distinct non-device username such as `bkmap-collector`, which cannot
match the 32-hex rule.

`client_id` and username are the same stable 32-character lowercase hex
device ID in the compact topic representation. They are generated from the
random stable UUID, not the display name. The password, not the username or
client ID, is the secret. Mosquitto client IDs must be unique among connected
clients; a second connection with a colliding ID may evict the first, so the
collector/broker logs should flag duplicate-client connections. The exact
per-user ACL still confines a device credential to its own device topics.
Credential rotation replaces the password without changing device UUID/topics
unless identity is reset.

## 7. Enrollment and credential lifecycle

### Recommended enrollment: operator-generated unique broker credentials

For the first self-hosted deployment, avoid a custom Mosquitto plugin and
avoid an always-on HTTPS credential-issuance endpoint. After the collector
and broker administration tooling exist:

1. Owner selects **Add Device** in the authenticated dashboard/operator tool.
2. Provisioning generates a device UUID and 256-bit random broker password,
   adds a salted verifier and a matching per-device ACL entry, and produces a
   one-time enrollment record. It does not enable telemetry.
3. The owner enters the device identity/username, password, and display name
   in the map's authenticated local UI over a trusted/private LAN. Do not
   place credentials in URL, browser query, logs, serial output, or support
   exports. Current local UI is HTTP, so a trusted LAN/on-path risk must be
   disclosed; do not claim local credential transport encryption.
4. Device stores credentials only in `bkmap_mon`; enrollment state becomes
   “configured, not enabled.”
5. Owner explicitly reviews consent, enables monitoring, then requests a
   connection test. Connection test proves broker/TLS/auth reachability only;
   it does not report heartbeat data unless monitoring has been enabled.
6. Dashboard associates the published topic identity with its device record.

The proposed local UI fields are:

- Enable Remote Monitoring checkbox, unchecked by default and disabled until
  credentials are saved and disclosure is acknowledged.
- Device name, broker host, fixed production port 8884, username/device ID,
  password entry, and heartbeat interval.
- Enrollment/connection state (`Not configured`, `Configured`, `Connected`,
  `Error`), last successful publish, and last bounded error category.
- `Save credentials`, `Test connection`, `Enable`, `Disable monitoring`, and
  `Reset monitoring identity`.

Every mutation uses Phase 4V administrator authentication and CSRF protection.
`Test connection` verifies SNTP/TLS/broker authentication only; it does not
publish status unless monitoring is explicitly enabled. Display the configured
broker host and consent disclosure before enabling. Because the current local
UI uses HTTP, credential entry is exposed to on-path observers on that LAN;
the UI must state this and should be used only on a trusted/private LAN.

The enrollment options are:

- **A. Dashboard/operator creates credentials (recommended):** simple,
  no device enrollment endpoint; operator provisions broker and user pastes
  credentials into local UI. Requires trusted-LAN handling.
- **B. Short-lived HTTPS issuance endpoint:** reduces manual copying but adds
  an internet-facing issuance service, TLS/time policy, token replay/expiry,
  and revocation protocol. HTTPS would be enrollment-only, never telemetry.
- **C. Per-device mutual TLS certificates:** strong client TLS identity, but
  requires per-device certificate issue/renew/revoke and private-key storage;
  not recommended for first version.
- **D. Mosquitto Dynamic Security plugin/API:** could automate credentials and
  ACL lifecycle, but adds plugin/API administration and broader broker-control
  risk. Evaluate only if audited operator tooling is insufficient; not a v1
  prerequisite.

Manual operator provisioning is the recommended first design; no option is
implemented or approved yet.

### Mosquitto administration and atomic provisioning

The future provisioning tool must validate device IDs, generate random
credentials using a cryptographic RNG, update the password database and ACL
atomically, validate/reload broker configuration safely, and report success
only after broker authentication/ACL tests pass. Do not expose the password
file or admin credential to the dashboard web process. Handle rollback if
either credential or ACL update fails. Remove/revoke the broker account and
ACL immediately when a device is deleted. Do not put real credentials in
tracked config examples or this design.

### NVS namespace and reset

Use dedicated namespace `bkmap_mon`, separate from Wi-Fi, admin auth, route
configuration, DNS preference, and OTA trust. Proposed bounded values:

| Key | Type / max | Purpose |
|---|---|---|
| `schema` | `u8` | Monitoring storage schema |
| `enabled` | `bool` | Explicit consent; missing/invalid means false |
| `device_uuid` | 16 bytes | Stable random device identity |
| `mqtt_user` | ASCII string, 32 bytes | Compact UUID-derived username/client ID |
| `mqtt_pass` | 32 random bytes, encoded as 64 lowercase hex chars | Per-device broker credential |
| `device_name` | UTF-8, 96 bytes / 48 Unicode scalar values | Dashboard presentation |
| `interval_s` | `u16` | One of 15, 30, 60, 300 seconds |
| `broker_host` | ASCII, max 128 bytes | Default `mqtt.goldwein.net`; future admin override only if approved |
| `broker_port` | `u16` | Production fixed to 8884; never fall back to 1883 |

Use fixed-size structures and validated bounded copies; do not use unbounded
`String` ownership for persisted values. Device name accepts valid printable
UTF-8, rejecting NUL, control characters, and line breaks. It is not an
authentication identifier. NVS encryption is not enabled; physical extraction
can expose the password. Inspect NVS/partition headroom before implementation.

- **Disable Monitoring:** set `enabled=false` first; stop publishing and
  reconnect attempts immediately; preserve UUID, broker identity, password,
  endpoint, and settings for re-enable.
- **Reset Monitoring Identity:** disable first, then delete only `bkmap_mon`
  identity and credentials. Preserve Wi-Fi, admin credential, brightness,
  routes, DNS preference, and OTA trust. Owner/operator separately revokes
  broker account and ACL entry. A disconnected device cannot be remotely
  revoked until its broker credential is removed.
- Password rotation: provision new per-device password at broker, update
  device locally, test TLS/auth, then retire old credential; no shared fleet
  password and no OTA-key reuse.

## 8. Payload schema and privacy

JSON remains the payload format: bounded deterministic UTF-8, heartbeat target
at most 2 KiB and hard maximum 4 KiB. Do not add CBOR/protobuf unless measured
JSON cost later justifies it. Payloads contain protocol `brooklyn-telemetry`,
major `version: 1`, compact/UUID device identity, random boot ID, shared
monotonic sequence, uptime, `time_synced`, and `device_time_utc` (`null`
until synchronized). Serialize deterministically with no BOM, duplicate keys,
or whitespace; QoS 1 duplicate handling uses sequence identity.

Heartbeat schema remains
[`brooklyn-telemetry-v1.schema.json`](schemas/brooklyn-telemetry-v1.schema.json)
and event schema remains
[`brooklyn-event-v1.schema.json`](schemas/brooklyn-event-v1.schema.json),
adapted to the MQTT topic transport. The old HTTPS enrollment request and
service-response schemas are removed; availability is the literal UTF-8
payload `online` or `offline`, specified in the topic design. There is no HTTP
response or command schema.

Heartbeat fields: target/build/version; uptime/reset/boot count; free/minimum
internal heap and PSRAM; Wi-Fi connected and RSSI; DNS/SNTP health; MTA feeds
successful out of eight, mask if useful, poll outcome and last complete-feed
age; vehicles processed/resolved, occupied map LED and collision counts;
display state, logical brightness and route visibility mask; active
partition/OTA state; application-reported integrity and config checks; and
only actually supported safety/interlock/current-limit state. Unsupported
hardware measurements are `null`/unsupported, never invented.

Event payloads cover boot/reset, monitoring enabled, broker reconnect,
Fresh-to-degraded/degraded-to-Fresh, Stale, NoData, OTA pending/confirmed/
rollback, invalid configuration, critical memory, and supported physical
safety faults. No event for every normal feed poll.

| Field | Purpose | Sent when enabled? | Sensitivity | Retention |
|---|---|---:|---|---|
| Device UUID/topic ID and firmware/build | Associate reports and diagnose compatibility | Yes | Pseudonymous/low | While registered |
| User device name | Dashboard presentation | Yes | May reveal household | Editable; delete on unenroll |
| Boot ID, sequence, uptime, reset reason | Ordering and operational health | Yes | Low operational metadata | Latest state; event 30 days |
| Heap/PSRAM | Resource diagnostics | Yes | Low | Latest state |
| Wi-Fi state/RSSI, DNS/SNTP status | Connectivity diagnostics | Yes | Moderate presence metadata | Latest state |
| SSID, local/private IP, gateway, DNS IP | Not required | **No** | High network/context disclosure | Never collect |
| Wi-Fi/admin credentials, verifier, salt, cookies, CSRF | No telemetry purpose | **No** | Secret | Never collect |
| Feed counts, poll/display state, aggregate vehicle/LED/collision counts | Pipeline health | Yes | Low operational metadata | Latest state |
| Raw GTFS, trip/vehicle IDs, passenger/user data | Not required | **No** | Linkable/sensitive | Never collect |
| Brightness/route visibility | Optional display diagnostics | Disclose; owner may opt out | User preference | Latest state |
| OTA state, partition, application-reported checks | Update health | Yes | Security-sensitive metadata | Latest state; events 30 days |
| OTA private key or monitoring password | Authentication secrets | **No** | Critical secret | Never collect/log |
| Browser history or arbitrary LAN discovery | No telemetry purpose | **No** | High | Never collect |

Before enabling, local UI discloses the configured broker host and fields
sent. Proposed wording:

> Remote Monitoring sends operational status and diagnostics from this
> Brooklyn Map to the configured Mosquitto monitoring service. It does not
> send Wi-Fi passwords, administrator credentials, raw subway-feed data, or
> browsing information. Monitoring can be disabled at any time.

## 9. Cadence, reconnect, queue, and MTA priority

### Heartbeat and availability timing

Choices: 15, 30, 60, or 300 seconds; default when enabled: 30 seconds.
Connection keepalive: 60 seconds, independent of heartbeat. Retained status
is overwritten by each newest complete heartbeat; do not retain heartbeat
history or each event.

Collector online rules:

- OFFLINE if retained availability says `offline`, or status age is greater
  than `3 × interval + 30 seconds`.
- ONLINE only if availability says `online` and latest accepted status age is
  no greater than `2 × interval + 15 seconds`.
- LATE in between, including retained `online` with overdue status.

If broker is down, collector's prior SQLite receive timestamp still ages;
replayed retained status after restart does not reset its original receipt
timestamp. If the collector has never seen a status, classify unknown/offline
until a fresh publication arrives.

### Reconnect and event queue

Keep a persistent connection while enabled and TLS time is valid. On disconnect
use exponential backoff with full jitter: initial ceiling 5 seconds, double
to a 15-minute cap. Reset backoff only after successful CONNACK and
availability/status PUBACKs. On Wi-Fi reconnect, attempt promptly subject to
anti-storm limits and scheduler/heap availability. TLS/DNS/connect timeouts
must be bounded (design target 4 seconds for DNS/connect/TLS); do not block the
main loop waiting.

Keep latest state and a 16-entry RAM-only important-event ring. No persistent
NVS telemetry queue. On reconnect publish current availability online, then
latest retained status, then bounded recent events in sequence order. Coalesce
identical transitions within 60 seconds; ordinary event spacing at least 10
seconds and maximum 12/minute. Critical supported safety/rollback events may
bypass spacing but must remain bounded. Drop/coalesce oldest low-priority
events on overflow; current truth is more important than exhaustive replay.

QoS 1 means duplicate delivery is possible; collector deduplicates `(device,
boot, sequence)` and applies status only if newer. No unlimited broker offline
queue. Set MQTT session clean/no persistent per-device subscription session.
Do not configure a huge per-client queue.

### Scheduling / non-interference

Telemetry is lower priority than safety, display, MTA fetch/decode, local UI,
and OTA safety. Current Phase 4J networking is synchronous and can block the
main loop during MTA cycles. Do not run MQTT connect/publish in a display path,
feed decoder, or UI handler, and do not retune MTA timeouts.

Implementation must use a bounded MQTT worker/service mechanism with explicit
network/heap budgeting and no inbound subscription. MQTT keepalive servicing
must remain timely even while feed operations occur; measure coexistence of
feed TLS and persistent MQTT TLS sockets. If one cannot meet the deadline,
defer telemetry without affecting the map. Initial design budgets: at most 4s
for DNS/connect/TLS, bounded publish-ack wait (2s design target), one bounded
operation per scheduler slot, and no inline retry loop. The implementation
must demonstrate how those limits hold with its actual MQTT library.

Telemetry outage never causes reboot, Wi-Fi reset, credential clearing,
provisioning, MTA timing changes, or display interruption.

## 10. Mosquitto, collector, SQLite, and dashboard

### Broker

Reference broker: `mqtt.goldwein.net:8884`, public trusted server certificate,
anonymous access disabled, TLS-only internet telemetry, unique per-device
authentication, strict ACL, retained messages enabled, persistence for
retained availability/status as appropriate, bounded connection/message
limits, and security-relevant logs without passwords/payload secrets.
Illustrative config and ACL examples:

- [`mosquitto-brooklyn.conf`](examples/mosquitto-brooklyn.conf)
- [`mosquitto-brooklyn.acl`](examples/mosquitto-brooklyn.acl)

They are templates only, not validated or applied. No domain, DNS, firewall,
Mosquitto, certificate, authentication, or Apache changes have been made.

### Collector and SQLite

Separate subscriber account reads only the three telemetry topic patterns and
cannot publish device topics. Collector validates topic ID against payload ID,
schema/version/bounds, broker-authenticated username identity where exposed,
sequence/dedup, and allowed event enums. MQTT delivery is the telemetry
transport; no HTTPS device endpoint is required for telemetry or enrollment
in the recommended first design.

Minimal SQLite tables:

- `devices`: device ID, owner, display name, target, broker username/key
  reference (never plaintext password), enrolled/revoked time.
- `latest_state`: one latest validated heartbeat JSON and server receive time
  per device; upsert only for newer sequence.
- `availability`: current broker state and transition receive time.
- `events`: normalized bounded event and broker receive time.
- `dedupe`: recent boot/sequence identifiers and retained boot history needed
  to reject duplicates; bounded according to the topic design.

Use SQLite transactions and WAL; keep the database off the public web root,
restrict filesystem permissions, back up securely, and do not store routine
heartbeat history by default. Retain latest state while registered, events
for 30 days, and only minimal enrollment/revocation audit. Remove device
records/latest state/events on owner deletion subject to backup deletion
policy. Collector persists first receipt timestamps so broker-retained replay
does not make an old device appear fresh.

### Dashboard and Apache

`https://monitor.goldwein.net/` is the human-facing dashboard. Apache serves
static content or reverse-proxies to the application over loopback/private
network; TLS and normal web security apply. The dashboard requires human
authentication and per-user authorization for every device/history view;
prefer a supported identity provider/MFA or separately secured MFA accounts.
Never reuse the map's local administrator password. Device MQTT credentials
and dashboard human credentials are distinct.

Show device name, ONLINE/LATE/OFFLINE, HEALTHY/WARNING/FAULT, last seen,
firmware/build, app-reported OTA/integrity, uptime/reset reason, RSSI, DNS/time
sync, MTA feeds x/8 and complete-feed age, display state, processed/resolved,
heap/PSRAM, safety state if supported, and bounded event history. Status uses
text plus visual distinction, not color alone. There are **no control
buttons**.

Suggested health severity: OFFLINE is a connectivity state; for devices
reporting, invalid config/explicit safety fault/failed self-check/prolonged
NoData = FAULT; degraded/stale/pending OTA/rollback/low memory = WARNING;
recent Fresh with valid config and no warning = HEALTHY. Define exact
thresholds from real MTA polling behavior before implementation.

## 11. Broker outage/restart, retention, and optional integrations

Mosquitto restart may interrupt connections; devices reconnect with backoff,
republish retained `online`, then latest retained status, then bounded events.
The collector receives retained state but preserves SQLite's earlier receive
time when the payload is a duplicate. A retained `online` is not proof of a
live device. Last Will may not be published if broker itself is down; heartbeat
age continues to determine stale/offline status until broker and device
reconnect.

Retention: latest status while registered; routine heartbeat history not
stored; events 30 days; availability current state plus useful transitions;
broker persistence limited to retained status/availability and configured
operational needs. Bound SQLite dedupe history and event storage. No unlimited
time-series store.

Home Assistant compatibility is optional future benefit of MQTT only. Do not
require Home Assistant, adopt its discovery schema in v1, or make it part of
Phase 4AA implementation.

Automatic updates stay separate. Dashboard may independently display that a
newer firmware release exists. There is no MQTT update, firmware, command, or
control topic; Phase 4W signed OTA remains the trust foundation and no
telemetry message can initiate it.

## 12. Threat model and limitations

| Threat | Mitigation | Residual risk |
|---|---|---|
| Network observer | TLS with hostname/chain validation; no plaintext fallback | Timing, broker hostname, traffic volume remain visible |
| Unauthorized device | Unique random credentials, broker auth, strict own-topic write ACL | Stolen device password can impersonate until revoked |
| Malicious MQTT client | Device has no subscribe permission/callback; no command topics | Broker/admin compromise remains in trust boundary |
| Broker/service compromise | Minimize payload, ACL, host hardening, no secrets/raw GTFS, limited retention | Broker can read/alter/drop telemetry and impersonate service to clients |
| Physical extraction | Per-device credential, revocation/rotation; no shared fleet password | NVS not protected by Secure Boot/Flash Encryption; password may be extracted |
| QoS/replay/duplicate | Boot ID + sequence, collector dedupe and monotonic upsert | Cloned credential can publish valid fresh sequence |
| Compromised dashboard account | MFA, scoped device authorization, CSRF/rate limits, audit | Account can view authorized telemetry |
| Over-retention | latest state only, events 30 days, deletion process | Backups/logs need aligned deletion windows |
| Secret leakage | Never log password; redact config/serial; schema allowlist | Future implementation regressions need tests/review |
| Endpoint outage/DoS | bounded reconnect/backoff, RAM event ring, map priority | Remote view becomes late/offline; map operation continues |

MQTT per-device authentication gives broker-level publisher identity, not
end-to-end proof to an arbitrary downstream recipient. Since the broker is
trusted in this architecture, P-256 payload signing is not required for the
first implementation.

## 13. Resource estimate (not measured)

Current production network image baseline: 93,308 B static RAM /
1,116,665 B flash. Existing MTA buffer is 512 KiB in PSRAM; Phase 4J reports
TLS internal-RAM use was not measured. MQTT adds a persistent socket/TLS
session plus client buffers rather than short-lived HTTPS sessions. Planning
budget (must be measured; not a build claim):

| Resource | Planning estimate |
|---|---:|
| Monitoring NVS values | Under 1 KiB payload; 1–2 KiB including metadata |
| State/task/event ring | 4–10 KiB RAM |
| JSON buffer | 4 KiB maximum; <=2 KiB typical |
| MQTT client buffers/task | 8–20 KiB RAM, library-dependent |
| Persistent TLS session | Reserve 40–80 KiB internal RAM; measure under reconnect and feed TLS overlap |
| P-256 payload signing | Not used in v1; no signature/key-operation buffer |
| Incremental peak envelope | Approximately 56–116 KiB internal RAM, high uncertainty |

TLS persistence can hold internal heap while MTA TLS uses additional resources.
Before implementation, measure minimum/largest internal free heap under
simultaneous/scheduled feed processing, MQTT keepalive/publish/reconnect, and
low-memory conditions on the authorized XIAO bench target. Do not infer
production hardware safety from XIAO. Abort/defer MQTT cleanly if budgets are
not available; do not use PSRAM as an assumed TLS internal-RAM fallback.

## 14. MQTT versus the superseded HTTPS design

| Area | MQTT/TLS (selected) | HTTPS POST (superseded) |
|---|---|---|
| Hosting | Natural fit for existing Mosquitto preference; local subscriber fan-out | Simple request API; would require an HTTPS telemetry collector endpoint |
| Telemetry | Persistent publish-only connection; retained state and events | Separate request per heartbeat/event; no native retained status |
| Availability | Last Will plus heartbeat age | Server infers availability only from request age |
| Device complexity | MQTT client/session, keepalive, QoS1, topic ACLs | Existing HTTPS client, request retry, HTTP endpoints |
| Dependencies | Likely MQTT client component/library; must select and document | Existing HTTP/TLS stack can be reused |
| RAM | Persistent TLS session held; may overlap feed TLS | TLS/request often transient but still significant peak |
| Authentication | Broker username/password + TLS + ACL; no payload signature recommended | Per-device application signature and enrollment/API validation previously proposed |
| Server | Broker + subscriber/collector + SQLite + dashboard | HTTPS API + collector + SQLite + dashboard |
| Offline/restart | LWT and retained state; QoS1 duplicate handling required | Retry requests; no Will, retained state, or broker fan-out |
| Security boundary | Broker is trusted and can read/alter/forge delivery | HTTPS endpoint is trusted; body signature can add app-level authenticity |

MQTT is recommended because it matches the approved self-hosted broker model
and availability requirements. The principal new costs are persistent TLS RAM,
dependency/library selection, and broker administration. If those fail
measured resource or reliability constraints, stop and bring evidence back for
an owner decision; do not silently revert or fall back to plaintext.

## 15. Staged implementation plan (not authorized/executed)

**Stage 1A — Broker configuration and synthetic devices**

- First run a separately authorized infrastructure preflight; treat current
  broker as unverified.
- If needed, provision Mosquitto TLS listener 8884 with trusted certificate,
  anonymous off, per-device credentials/ACL, retained state, bounded limits,
  persistence/log policy. Port 1883 may be diagnostic-only; never production
  device fallback.
- Test synthetic devices for own-topic publish, cross-device denial,
  subscription denial, QoS1 duplicate handling, Will, retained messages,
  TLS hostname/chain failure, restart, and credential revocation.

**Stage 1B — Collector, SQLite, dashboard**

- Implement read-only subscriber and validate topics/schema/sequence.
- Implement SQLite latest-state/event/availability/retention and restart age
  semantics.
- Implement authenticated dashboard on Apache HTTPS with no control actions.
- Test tenant isolation, delete/retention, broker outage/restart, and stale
  retained messages.

**Stage 2 — ESP32 MQTT sender/configuration/enrollment**

- Only after broker/collector decisions and library/resource audit: add
  default-off configuration, Phase 4V-protected local UI, `bkmap_mon` storage,
  explicit consent, bounded worker, MQTT/TLS, PUBLISH-only ACL identity,
  retained status/availability, event ring, and serial diagnostics.
- No remote command support. Document the added dependency/version.
- Host-test schemas/state/backoff/disable/revocation/privacy; clean-build all
  four environments and repeat ELF physical-output audits.

**Stage 3 — XIAO hardware validation**

- Requires explicit authorization and verified broker infrastructure.
- Validate certificate trust/time gate, authentication/ACL, retained status,
  QoS1, Will, keepalive, reconnect/restart, disable/reset, heap, latency, and
  MTA/display non-interference on XIAO. Never infer production wiring from
  XIAO; production PCB remains separately unvalidated and requires approval.

## 16. Decisions still requiring owner approval

Before any implementation, Joel must approve:

- Broker preflight findings and whether `mqtt.goldwein.net:8884` is actually
  configured securely. Exact infrastructure steps if absent are in
  [`PHASE4AA_MQTT_TOPIC_SCHEMA.md`](PHASE4AA_MQTT_TOPIC_SCHEMA.md).
- Mosquitto version/support for ACL pattern substitution, forced client ID,
  persistence, and chosen auth mechanism.
- Manual credential enrollment and the risk of entering credentials through
  the existing local HTTP UI, versus a separately secured issuance service.
- Final MQTT library and measured persistent-TLS RAM/task behavior.
- Heartbeat choices/default, keepalive, event policy, retention, consent
  wording, and optional display-preference fields.
- Whether future payload signing or mutual TLS is needed after broker threat
  review; neither is recommended for initial v1.

## 17. Design artifacts and current status

- This document: MQTT architecture, lifecycle, privacy, threat model,
  resource budget, comparison, and staged plan.
- [`PHASE4AA_MQTT_TOPIC_SCHEMA.md`](PHASE4AA_MQTT_TOPIC_SCHEMA.md): topic,
  QoS/retain, Will, status timing, preflight, and operator setup guidance.
- [`brooklyn-telemetry-v1.schema.json`](schemas/brooklyn-telemetry-v1.schema.json):
  heartbeat payload.
- [`brooklyn-event-v1.schema.json`](schemas/brooklyn-event-v1.schema.json):
  event payload.
- [`brooklyn-availability-v1.schema.json`](schemas/brooklyn-availability-v1.schema.json):
  `online` / `offline` payload.
- [`mosquitto-brooklyn.conf`](examples/mosquitto-brooklyn.conf) and
  [`mosquitto-brooklyn.acl`](examples/mosquitto-brooklyn.acl): illustrative
  examples only; not deployed or validated.

All are design artifacts. No MQTT firmware, monitoring server, dashboard,
broker setup, Apache/DNS/firewall change, dependency, automatic update, or
hardware validation is included.
