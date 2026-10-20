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

## 3. Device ID decision (resolved)

**Decided:** the canonical device identity stays the frozen payload UUIDv4. The MQTT username, client ID and topic segment are `bkmap-<UUID>`. The earlier `bkmap-<12 hex>` proposal is **not** adopted and no schema change is made. Details, ACLs and the collector rule are in [PHASE4AA_PRODUCTION_PROVISIONING.md](PHASE4AA_PRODUCTION_PROVISIONING.md). The UUID is random, not derived from the MAC (a MAC leaks the manufacturer OUI and is enumerable).

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
