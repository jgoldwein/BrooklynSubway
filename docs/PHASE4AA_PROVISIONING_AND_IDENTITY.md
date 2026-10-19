# Phase 4AA provisioning and device identity (recommendations, not implemented)

Extends memos 2 and 3 in [PHASE4AA_DECISION_MEMOS.md](PHASE4AA_DECISION_MEMOS.md); does not contradict them. Frozen contracts are unchanged. **Nothing here is implemented; schema, firmware, broker and ACLs are untouched.**

## 1. Provisioning options

| Option | Summary | Strengths | Weaknesses |
| --- | --- | --- | --- |
| A. Manual | Operator creates the broker account and types the credential into the authenticated LAN UI | No new server component; fine for the first few units | Does not scale; operator handles secrets; error-prone |
| B. One-time HTTPS claim | Device (or LAN UI) redeems a single-use, expiring claim code over verified HTTPS; server returns a unique device ID and MQTT credential | Scales; server guarantees unique ID; credential never typed; supports revocation/rotation | Needs an enrollment service and ACL/account automation |
| C. Preprovisioned at manufacturing | Credentials burned in at flash time | Zero user steps | Secrets in the factory flow; flash encryption not enabled; reset/replacement hard |

**Recommendation (consistent with memo 2):** A for the first units, B for the product. C is not recommended unless flash encryption and a secure factory process exist.

## 2. Security requirements for any option

Unique strong per-device credential; per-device publish-only ACL on its own three topics, no subscribe; revocation and rotation path; claim codes single-use and expiring; claim over verified HTTPS (certificate bundle, no insecure mode); credentials never sent over MQTT; the `goldwein` admin account never used by devices or the collector; OTA signing keys entirely separate from MQTT credentials; factory reset clears the credential and the replacement flow issues a new identity and revokes the old.

**Physical-security caveat:** credentials in NVS (`bkmap_mon`) are not protected against someone with physical access, because flash encryption is not enabled. The blast radius is bounded: a stolen credential can only publish spoofed telemetry on that device's own three topics; it cannot read other devices' data or subscribe. Rotation/revocation restores trust.

## 3. Device ID recommendation (decision for Joel)

Frozen schema today: `device_id` and `boot_id` are UUIDv4 in payloads, and the topic segment is the compact 32-hex form. Joel's preference is `bkmap-<12 hex>` used consistently as MQTT username, client ID, topic segment and device ID.

Recommended: adopt **`bkmap-<12 lowercase hex>`** (48 random bits) as the single canonical string, **server-assigned at enrollment** (guaranteed unique) or device-generated with `esp_fill_random()` for manual units, persisted in NVS `bkmap_mon`. **Not derived from the MAC**: a MAC leaks the manufacturer OUI/hardware identity, is enumerable, and ties identity to hardware across a factory reset. Without a server uniqueness check the birthday collision probability at 10,000 devices is about 1.8e-7.

Username == client ID == device ID == topic segment lets one ACL pattern cover every device and prevents a device from impersonating another's topics by construction.

**Schema impact if adopted (needs Joel's approval; not done):** `device_id` pattern changes; topic-segment pattern changes from 32-hex; `boot_id` stays a UUIDv4; collector topic/payload matching, tests and examples change; a schema version decision (amend v1 before any device ships vs bump). **Alternative:** keep the UUID (lowest effort, already implemented and tested). Either is workable; choose before firmware is written.

## 4. ACL pattern note

Mosquitto `pattern` rules substitute `%u` (username) and `%c` (client ID):

    pattern write brooklyn/v1/%u/status
    pattern write brooklyn/v1/%u/event
    pattern write brooklyn/v1/%u/availability

With username == topic segment, one rule set serves all devices; the collector keeps explicit `topic read` rules. Pattern rules apply to every user but only grant write on that user's own topics, which is harmless. Existing `brooklynmap` already satisfies it. **Unverified:** the deployed Mosquitto version and its `%u` behavior; test on the broker before relying on it (a change Joel makes, not this repo).

## 5. Friendly names

Keep the friendly name as dashboard/server-side metadata keyed by device ID. It is presentation only, never an identity or authorization input. The existing optional `device_name` in the payload stays presentation-only and untrusted.

## 6. Host deployment security

Covered in [PHASE4AA_DEPLOYMENT.md](PHASE4AA_DEPLOYMENT.md): loopback-only dashboard behind Apache, separate service users, read-only dashboard DB access, backup/restore.
