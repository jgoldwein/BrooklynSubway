# Phase 4AA — MQTT Topic and Infrastructure Contract

**Design only. No broker configuration, DNS change, firewall rule, Apache
change, or device test has been performed.** Client-side protocol checks
against the broker have been run (below).

## 1. Production broker and infrastructure state

Production endpoint: `mqtt.goldwein.net:8884`, MQTT over TLS with server
certificate and hostname verification enabled.

Verified 2026-10-19 from Windows: DNS, TCP 8884, TLS 1.3, Let's Encrypt
certificate (valid 2026-10-10 through 2027-01-08) with hostname/chain
verification, MQTT 3.1.1, anonymous CONNECT rejected, authenticated CONNECT,
QoS 1, retained messages, and retained Last Will. Publish-only device ACL
isolation was verified with the temporary `brooklynmap` account on the 8884
`acl_file`: own-topic writes work; other-namespace and unrelated publishes are
not delivered; the device receives nothing (SUBACK is still granted by
Mosquitto, so verify by delivery, not SUBACK). The read-only `bkmap-collector`
ACL is also verified (nothing outside the three topic classes is delivered even
for `#`; its publishes are not delivered). **Not yet verified:** Mosquitto
version and per-device production provisioning.
The personal `goldwein` account is unrestricted and is not a device or
long-term collector identity.

Before any Phase 4AA implementation or hardware test, obtain explicit
authorization to perform this preflight and record:

1. DNS A and AAAA answers for `mqtt.goldwein.net` and `monitor.goldwein.net`;
   confirm they point to the intended host and are not stale.
2. TCP reachability of 1883 only as an optional diagnostic of the existing
   broker. This is not evidence of TLS and must never be used by firmware.
3. TCP reachability of 8884.
4. TLS handshake on 8884 with SNI `mqtt.goldwein.net`; validate CA chain,
   hostname, validity dates, and renewal path.
5. Anonymous login behavior (must be rejected) and authenticated client
   behavior using a temporary test identity; confirm invalid credentials are
   rejected.
6. Mosquitto version and effective listener/auth/ACL/persistence settings
   through authorized server access, without copying or exposing secrets.
7. ACL behavior: device can publish its own topics only; cannot publish other
   device topics or subscribe; collector can read telemetry only.
8. Broker restart, retained state, QoS 1, and Last Will behavior.

Possible non-secret diagnostic commands (do not include credentials on a
command line):

```text
dig +short A mqtt.goldwein.net
dig +short AAAA mqtt.goldwein.net
dig +short A monitor.goldwein.net
dig +short AAAA monitor.goldwein.net
nc -vz mqtt.goldwein.net 1883
nc -vz mqtt.goldwein.net 8884
openssl s_client -connect mqtt.goldwein.net:8884 -servername mqtt.goldwein.net -verify_return_error
mosquitto -h
systemctl status mosquitto
journalctl -u mosquitto --since "30 minutes ago"
```

Only run the 1883 probe as an infrastructure diagnostic. Never configure a
device to try 1883, including as a fallback when 8884 fails. Do not use an
unauthenticated probe that publishes data or exposes an actual device
credential. The `mosquitto -h` and system service commands require authorized
access to the Linux broker.

## 2. If TLS listener 8884 is absent

Stop before device testing. Do not attempt device fallback to 1883. An
authorized server operator should:

1. Create/verify DNS A/AAAA for `mqtt.goldwein.net` pointing to the broker host
   (and `monitor.goldwein.net` to the dashboard host if separate).
2. Obtain publicly trusted certificates with SANs for `mqtt.goldwein.net` and
   `monitor.goldwein.net`. Certificates may be issued through ACME/Apache, but
   Apache's HTTPS listener does not itself provide MQTT TLS; Mosquitto must
   load its broker certificate and private key.
3. Install Mosquitto with a version that supports the selected TLS, password
   file, ACL, retained messages, persistence, and connection-limit settings.
4. Configure a TLS listener on TCP 8884 with the broker certificate/key,
   `allow_anonymous false`, per-device password authentication, exact
   per-device write ACLs, a read-only collector account, retained-message
   support, persistence, bounded packet/connection limits, and security
   logging. Do not expose an unauthenticated listener.
5. Configure the host and cloud/provider firewalls: allow TCP 8884 to the
   Mosquitto TLS listener; allow TCP 443 to Apache for the dashboard (and TCP
   80 only if required for ACME validation/HTTPS redirect). Do not expose the
   collector/database port. Do not open router forwarding at the Brooklyn Map
   installation; this is an outbound client. Port 1883 may remain closed or
   be temporarily diagnosed, but is not a production endpoint.
6. Configure Apache HTTPS separately for `monitor.goldwein.net`, with its
   certificate, human authentication/authorization, and a reverse proxy over
   loopback/private network or static/dashboard deployment. Keep the
   application and SQLite database outside the public document root. Apache
   does not proxy telemetry from devices.
7. Validate Mosquitto configuration, restart in a planned window, inspect
   service logs, and repeat TLS/auth/ACL/Will/restart checks before approving
   ESP32 implementation/testing.

The tracked examples in `docs/examples/` are templates and must be adapted,
version-checked, secret-protected, and reviewed on the server. They have not
been applied or tested.

## 3. Topic hierarchy and ACL identity

The stable device UUID is stored in the payload in canonical lowercase
hyphenated form. For topic and broker username, remove UUID hyphens to obtain
exactly 32 lowercase hexadecimal characters:

```text
brooklyn/v1/<32-lowercase-hex-device-id>/status
brooklyn/v1/<32-lowercase-hex-device-id>/event
brooklyn/v1/<32-lowercase-hex-device-id>/availability
```

Example only:
`brooklyn/v1/0123456789abcdef0123456789abcdef/status`.

| Topic | Payload | QoS | Retain | Delivery rule |
|---|---|---:|---:|---|
| `.../status` | Heartbeat JSON, protocol `brooklyn-telemetry`, version 1 | 1 | Yes | Latest complete status overwrites previous retained status |
| `.../event` | Event JSON, protocol `brooklyn-telemetry`, version 1 | 1 | No | Significant transitions only; duplicates deduped by boot/sequence |
| `.../availability` | JSON string `"online"` or `"offline"` | 1 | Yes | Current state and Last Will |

No `meta` topic is proposed; device registry metadata belongs in the collector
and should not be duplicated as a retained topic. There are no command/control/
reboot/OTA/firmware/configuration topics.

Each device uses username = compact device ID and stable MQTT client ID =
compact device ID. The broker authentication secret is a unique, high-entropy
random per-device password; the username/client ID/device ID are identifiers,
not secrets. A provisioned password for one username must not grant any
permission for another device ID.

Example Mosquitto ACL policy for a device username `abc...` is exact topic
write permission for only that device's three topics. Do not rely on client
supplied topic IDs alone. The collector has a separate username with read-only
wildcard access to status/event/availability and no write access. A device has
no read permission, no SUBSCRIBE permission, and no MQTT message callback.
All unlisted ACL operations are denied.

## 4. Availability, QoS, and ordering

On every connection attempt the device configures its Last Will before
CONNECT:

- topic: its own `.../availability`
- payload: JSON string `"offline"`
- QoS: 1
- retained: true

After successful CONNACK:

1. Publish retained QoS 1 `"online"` and wait for PUBACK.
2. Publish current retained QoS 1 status and wait for PUBACK.
3. Publish bounded recent queued events at QoS 1, not retained.
4. Keep the connection and 60-second MQTT keepalive active. Do not reconnect
   for each heartbeat.

On intentional disable/shutdown, publish retained QoS 1 `"offline"`, allow a
bounded PUBACK wait, then send clean DISCONNECT. A clean DISCONNECT suppresses
the Will. If the offline PUBACK fails, do not claim the broker received it.
An unexpected connection loss causes Mosquitto to publish the retained Will
when it detects failure; this may take up to the keepalive/transport detection
window and is not instantaneous. QoS 1 may duplicate all three message types;
collector dedupe is required for status/events.

Retained status is the latest snapshot only. Do not retain every heartbeat or
event. A retained `online` or status received during collector subscription is
not proof of current device health. Use collector timestamps plus heartbeat age.
Store the first receive time per `(device, boot, sequence)` so retained replay
does not refresh a stale device. Inspect the MQTT RETAIN flag: retained
replay during subscription is not a new live status publication.

## 5. Deterministic availability rules

Let `I` be the enrolled heartbeat interval (15, 30, 60, or 300 seconds).

- `OFFLINE` immediately if latest retained availability is `offline`.
- `OFFLINE` if no accepted fresh status exists and status age exceeds
  `3 × I + 30 seconds`.
- `ONLINE` only if availability is `online` **and** latest accepted status age
  is at most `2 × I + 15 seconds`.
- `LATE` otherwise (for example availability online but status overdue).

`OFFLINE` is a collector-derived connectivity state. `HEALTHY`, `WARNING`, and
`FAULT` describe the last device-reported operational health independently.
If Mosquitto restarts and retained availability says online but the collector
has no trustworthy prior receive time, mark unknown/late until a new live
status arrives; do not convert retained replay into a fresh heartbeat.

## 6. MQTT client and reconnect budgets

- MQTT 3.1.1, clean session, keepalive 60 seconds.
- Persistent broker connection while monitoring enabled and SNTP/TLS valid.
- Before trustworthy SNTP: no MQTT/TLS connection attempt; monitoring remains
  pending, while the map/MTA/display/local UI continue normally.
- On disconnect: exponential backoff with full jitter, initial ceiling 5
  seconds, doubling to 15-minute cap. On Wi-Fi availability, retry promptly
  subject to anti-storm/resource limits.
- DNS/connect/TLS operation budget: target at most 4 seconds; publish/PUBACK
  target at most 2 seconds. These are design targets pending library evidence.
- Never block the display/main path or feed processing waiting for MQTT.
- No automatic fallback to port 1883, alternate insecure hostname, or
  certificate bypass.
- Do not retain an unlimited offline publish queue. Keep latest snapshot and
  a fixed 16-entry RAM event ring; publish latest status first when reconnected.

## 7. Payload schemas and privacy

JSON schemas:

- `docs/schemas/brooklyn-telemetry-v1.schema.json`: heartbeat status.
- `docs/schemas/brooklyn-event-v1.schema.json`: significant event.
- `docs/schemas/brooklyn-availability-v1.schema.json`: availability string.

Heartbeat interval choices: 15, 30, 60, 300 seconds; recommended enabled
default 30 seconds. Heartbeat payload target <=2 KiB; hard max <=4 KiB.
Payload common fields include protocol/version, device ID, random boot ID,
increasing sequence, uptime, SNTP synchronization, and nullable UTC timestamp.
Status includes build/target, reset, internal heap/PSRAM, Wi-Fi state/RSSI,
DNS/SNTP health, aggregate MTA success/poll/feed-age/vehicle/LED counts,
display state/logical brightness/route summary, OTA/config/application-reported
integrity, and only safety measurements supported by actual hardware.

Event types are enumerated: boot/reset, monitoring enabled, broker reconnect,
Fresh-to-degraded/degraded-to-Fresh, Stale, NoData, OTA pending/confirmed/
rollback, configuration invalid, critical memory, and supported production
safety fault. Coalesce repeated events within 60 seconds; ordinary events at
least 10 seconds apart and max 12/minute; critical transitions may bypass
spacing but remain bounded.

Never publish SSID, local/private IP, gateway/DNS IP, Wi-Fi/admin credentials,
verifier/salt/cookies/CSRF, broker password, OTA private key, raw GTFS, complete
trip/vehicle IDs, passenger/user data, browsing information, arbitrary LAN
discovery, or raw logs. User name, brightness, and route preferences require
explicit local disclosure; owner should be able to omit preferences.

## 8. Collector and server contract

Subscriber subscribes only to:

```text
brooklyn/v1/+/status
brooklyn/v1/+/event
brooklyn/v1/+/availability
```

Collector runs as a separate low-privilege service. Validate topic shape,
topic/payload identity match, JSON schema/version/size, event enum, monotonic
sequence, QoS duplicates, and device registry. It has no broker publish
permission. SQLite stores:

- devices: owner, display name, device ID/username, registration/revocation;
- latest_state: latest valid status and original broker receipt time;
- availability: latest availability and transition times;
- events: bounded normalized events, 30-day retention;
- dedupe: bounded sequence/boot replay metadata.

Retain latest state while registered, no routine heartbeat history, events
30 days, current availability, and minimal enrollment/revocation audit.
SQLite data stays outside Apache's public document root, with restrictive
filesystem permissions and secure backups. Delete device state and events on
owner deletion subject to documented backup deletion window.

Apache provides HTTPS for `monitor.goldwein.net`, serving or reverse-proxying
the authenticated dashboard. Require human login and per-device authorization;
prefer MFA/established identity provider. Do not reuse map admin credentials.
Dashboard shows device name, online/late/offline, healthy/warning/fault,
last-seen, version/target, application-reported integrity/OTA, uptime/reset,
RSSI/DNS/time sync, MTA x/8/feed age/display/processed/resolved, heap/PSRAM,
supported safety status, and recent events. No remote-control buttons.

Mosquitto restart: devices reconnect with backoff, republish online then latest
status then bounded events. Collector preserves the earlier SQLite receipt
time for retained duplicate status, so restart does not make stale devices
fresh. Last Will may not be delivered while broker is down; heartbeat age is
the secondary offline signal.

Home Assistant is optional future integration only. No HA discovery topics or
dependencies are required. MQTT is not an update channel; dashboard may show a
newer version independently, but Phase 4AA has no update/firmware/command
topic.

## 9. Mosquitto/DNS/Apache/firewall setup if missing

No infrastructure changes are authorized by this document. If preflight
shows no secure listener, the future authorized operator must:

1. Correct DNS A/AAAA for `mqtt.goldwein.net` and `monitor.goldwein.net`.
2. Obtain a trusted certificate with `mqtt.goldwein.net` in SAN; install the
   full certificate chain and private key with Mosquitto-readable,
   least-privilege filesystem permissions. Apache may own ACME issuance but
   Mosquitto must load the certificate itself.
3. Configure Mosquitto listener 8884 only for production telemetry, TLS
   certificate/key, anonymous access off, password file, exact ACLs, retained
   messages, persistence, message/connection limits, and security logs.
4. Provision the collector read-only account; per-device accounts receive
   only exact own-topic write ACLs.
5. Allow inbound TCP 8884 at host/cloud firewall to Mosquitto. No forwarding
   or inbound rule is needed at the device/home location. Keep port 1883
   closed for production; if diagnosed temporarily, do not connect firmware.
6. Configure Apache HTTPS separately for the dashboard with human
   authentication/authorization; Apache is not an MQTT proxy.
7. Validate service startup/logs, TLS chain/hostname/date, anonymous rejection,
   valid and invalid credentials, own-topic and cross-topic ACL, denied
   subscribe, QoS 1, Will, retained messages, persistence, and broker restart.
8. Only after all checks pass should a separate implementation/hardware test
   be considered.

Example Mosquitto templates are under `docs/examples/`; they contain no live
credentials and are not production-ready until version-checked and reviewed.
The ACL and configuration directives should be checked against the deployed
Mosquitto version's official `mosquitto.conf(5)` and `mosquitto(8)` manuals
before use.
