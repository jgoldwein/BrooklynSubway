# Phase 4AA production identity, ACL and provisioning design

Documentation only. **Nothing here is installed or implemented**: no live broker, ACL or account change, no production credential, no NVS or UI code, no eFuse or flash-encryption change. Any live broker change needs Joel's explicit approval first.

## 1. Approved naming convention

| Item | Value |
| --- | --- |
| Canonical device ID (frozen schema) | `<UUID>`, lowercase hyphenated UUIDv4 |
| MQTT username | `bkmap-<UUID>` |
| MQTT client ID | `bkmap-<UUID>` |
| Topic root | `brooklyn/v1/bkmap-<UUID>/` |
| Payload `device_id` | `<UUID>` |
| Friendly name | Separate metadata; never used for authentication or authorization |

The UUID is random, not derived from the MAC address.

## 2. Collector rule (implemented, host-tested)

For `brooklyn/v1/bkmap-<UUID>/{status,event,availability}` the collector takes `<UUID>` from the topic and requires the payload `device_id` to match exactly (`ingest.Collector.resolve_topic_identity`). It rejects (`rejected:bad_topic`, or `rejected:device_id_mismatch` for a payload mismatch):

- a wrong prefix or version, a bare UUID, a malformed, short, uppercase or non-v4 UUID, trailing text, or a friendly name
- an extra path component, an empty or unsupported topic class, or a wildcard segment

Nothing is normalized. No alias table is needed for production devices.

The temporary bench account `brooklynmap` works only through the explicit `--test-alias brooklynmap=<synthetic UUID>` option. An alias may not use the `bkmap-` namespace, must target a canonical UUID, and is empty by default, so an unconfigured collector rejects `brooklynmap`.

## 3. Exact draft ACL for one hypothetical device (do not install)

Hypothetical UUID `123e4567-e89b-42d3-a456-426614174000`:

```text
user bkmap-123e4567-e89b-42d3-a456-426614174000
topic write brooklyn/v1/bkmap-123e4567-e89b-42d3-a456-426614174000/status
topic write brooklyn/v1/bkmap-123e4567-e89b-42d3-a456-426614174000/event
topic write brooklyn/v1/bkmap-123e4567-e89b-42d3-a456-426614174000/availability
```

There is no `topic read`, no `topic readwrite`, no subscribe permission, and no wildcard or other-device write. The Last Will publishes to the `availability` topic, which the write rule already covers.

The collector account `bkmap-collector` remains read-only:

```text
user bkmap-collector
topic read brooklyn/v1/+/status
topic read brooklyn/v1/+/event
topic read brooklyn/v1/+/availability
```

Mosquitto does not reliably deny a subscription at SUBACK time, so isolation must be verified by message delivery with a trusted observer, not by SUBACK/PUBACK.

## 4. Optional future pattern ACL (not assumed to work)

```text
pattern write brooklyn/v1/%u/status
pattern write brooklyn/v1/%u/event
pattern write brooklyn/v1/%u/availability
```

Because the username is `bkmap-<UUID>`, `%u` would confine each account to its own namespace with one rule set. Explicit per-device ACLs remain acceptable. **Phase 4AB checklist item: verify the installed Mosquitto version and `%u` pattern behavior on the broker (by delivery) before relying on it.**

## 5. Credential storage design (proposal, not implemented)

Recommended location: ESP32 NVS. Because flash encryption is off, NVS must not be described as secure against physical extraction.

Proposed namespace `bkmap_mon` (already reserved by the design and FIRMWARE_INTEGRATION; 9 chars, NVS limit is 15). Keys (limit 15 chars):

| Key | Type | Content |
| --- | --- | --- |
| `cfg_ver` | u8 | Record version, starts at 1 |
| `device_id` | string | Canonical UUID (36 chars) |
| `mqtt_user` | string | `bkmap-<UUID>`; must equal `bkmap-` + `device_id` |
| `mqtt_pass` | string | Unique high-entropy password |
| `configured` | u8 | Written last, after the other keys are valid |

Never stored here: the `goldwein` admin credential, any shared production credential, the collector credential, or OTA signing keys. Each map has one unique credential.

Validation on load: `device_id` is a canonical UUIDv4, `mqtt_user == "bkmap-" + device_id`, the password is non-empty and bounded, and `configured == 1`. Any failure leaves telemetry off with a logged reason and does not fall back to the bench credential. The telemetry service already stays inactive when no credential is present.

## 6. Manual provisioning workflow for the first internal units (proposal)

1. The board boots without a production MQTT credential; telemetry is off and the LAN admin UI stays available.
2. An authenticated local administrator enters the UUID, MQTT username and MQTT password.
3. The device validates syntax and that the username matches `bkmap-<UUID>`.
4. The device writes the NVS record, with `configured` written last.
5. The password is never rendered back to the browser or UI, and never logged.
6. A reboot or reload activates telemetry.
7. Local status may show telemetry configured yes/no, the device ID and the username, but never the password.
8. Replacing the password requires an authenticated local admin.
9. Clearing the MQTT configuration requires an explicit authenticated local action.

Joel creates the broker account, the strong random password and the matching ACL by hand, and must approve before any broker change. This needs a new NVS schema and UI, so it requires Joel's review before any code is written.

## 7. Future product provisioning

The recommended long-term design is a one-time HTTPS claim. A short-lived, single-use claim code is redeemed over verified HTTPS. The service creates the device identity, a unique high-entropy password, the broker account and its publish-only ACL. It returns the credential, invalidates the claim, and supports revoke and re-enroll. It is not part of Phase 4AA.

## 8. Flash encryption and Secure Boot memo

The firmware does **not** use ESP32 flash encryption. An attacker with sufficient physical access may be able to extract MQTT credentials stored in NVS.

Current containment:

- one unique credential per device
- publish-only ACL
- own namespace only
- no MQTT subscriptions
- no remote command channel

A stolen credential can mainly spoof telemetry for that one device.

Flash encryption and Secure Boot are **not** enabled now and no eFuse is burned. Before wider production deployment, evaluate separately, first on expendable hardware:

- ESP32 flash encryption
- Secure Boot
- the manufacturing and key-management workflow
- OTA implications
- recovery and replacement implications
- development and debug implications

They are not prerequisites for Phase 4AB USB-only bring-up.

## 9. DNS health

`dns_health` remains `unknown`. The only DNS state in the firmware is the network-diagnostics snapshot, which runs only when a user triggers it from the LAN UI and is private to `NetworkService`. Reporting it would present stale diagnostics as health, and a continuous probe would add network traffic and change `NetworkService`, which is out of scope. A future change could expose a real health signal once one exists.
