# Phase 4AA decision memos (recommendations only; nothing implemented or deployed)

## 1. Dashboard authentication

Evaluated first model: Internet -> HTTPS :443 -> Apache on `monitor.goldwein.net`
(Apache authentication) -> reverse proxy -> `dashboard.py` bound to 127.0.0.1.

| | Apache Basic Auth over HTTPS | Application sessions (login form, cookies, CSRF) |
| --- | --- | --- |
| Attack surface | Auth handled by a hardened, widely reviewed component before any app code runs | New login, session, CSRF, reset and rate-limit code to write and maintain |
| Complexity | A few config lines plus `htpasswd` (bcrypt) file | Significant; storage, expiry, logout, audit |
| Fit for dashboard | Dashboard is read-only, GET-only, no state-changing endpoints, so CSRF exposure is nil | Over-engineered for 1-few viewers |
| Weaknesses | No logout, no MFA, browser caches credentials, brute force needs `mod_evasive`/fail2ban/rate limit | Better UX, MFA possible |
| Migration | Can later add Apache `mod_auth_openidc`/client certs without app changes | Locked into app |

**Recommendation:** start with Apache Basic Auth over HTTPS only (valid
Let's Encrypt cert, HTTP redirected, HSTS), a long random password in a
bcrypt `htpasswd` file outside the web root, `Require valid-user`, fail2ban
or Apache rate limiting on 401s, the dashboard bound to 127.0.0.1 and run
as an unprivileged systemd user, and the existing CSP/`nosniff` headers
preserved by the proxy. Never expose the dashboard port, the collector, or
the SQLite file. Revisit (OIDC or client certificates at Apache) only if
multiple viewers or MFA become requirements. Do not edit Apache until Joel
approves.

## 2. Production device provisioning

Required per map: unique stable device ID, unique MQTT username, unique
high-entropy password, matching Mosquitto ACL (publish-only to its three
topics), individually revocable.

| Criterion | A. Manual credentials via local authenticated map UI | B. One-time HTTPS claim/enrollment | C. Factory-provisioned per-unit credentials |
| --- | --- | --- | --- |
| Security | Password handled by a human; weak ones possible, copy/paste exposure | Device generates/receives a unique secret over TLS; token single-use and expiring | Good if factory process is secure; secrets exist at manufacturing |
| User effort | High (create broker user, edit ACL, type into map) | Low (enter short claim code) | None |
| Server complexity | None beyond Mosquitto admin | New enrollment service that can create broker users/ACL entries (a privileged component) | Provisioning tool at build time |
| Rotation/revocation | Manual per device | Re-claim issues new secret; revoke by deleting user | Manual or re-flash |
| Adding maps | Manual each time | Scales | Scales |
| Factory reset | Must re-enter | Re-claim (needs new code) | Credentials lost on NVS erase unless stored in a protected partition |
| NVS extraction | Secret is in NVS; physical access extracts it | Same | Same |

NVS extraction applies to all options: anyone with physical access to a unit
can read its MQTT credential unless flash/NVS encryption is enabled (a
separate hardware decision). Containment relies on ACLs: a stolen credential
can only publish that one device's own three topics and cannot subscribe.

**Recommendation:** B for the product (one-time HTTPS claim code issued by
the operator, exchanged for a unique device ID, MQTT username and password,
stored in NVS), implemented as a small separate enrollment service that is
the only component allowed to edit Mosquitto users/ACLs (dynamic security
plugin or generated `password_file`/`acl_file` plus reload). Use A only for the
first few internal units. Defer until the Mosquitto version is recorded and
whether to use Dynamic Security is decided. Enable ESP32 flash/NVS encryption
if physical credential theft becomes in scope.

## 3. XIAO ESP32-S3 `esp-mqtt` bench experiment (not implemented, not flashed)

> Superseded in detail by [PHASE4AA_BENCH_PLAN.md](PHASE4AA_BENCH_PLAN.md) (stages 0-10). Provisioning/device-ID refinements: [PHASE4AA_PROVISIONING_AND_IDENTITY.md](PHASE4AA_PROVISIONING_AND_IDENTITY.md).

Target: XIAO ESP32-S3 bench environment only (never inferred as production
wiring). No physical LED output (`LED_ENABLE` stays LOW / output build off). No `setInsecure()`.
Separate throwaway PlatformIO env/branch, not `platformio.ini` production.

Setup: Wi-Fi connected, valid SNTP before any MQTT; broker `mqtt.goldwein.net:8884`;
bench credentials only via a local ignored `secrets.h` (never committed), using the temporary
publish-only `brooklynmap` account; CA via the ESP-IDF certificate bundle or pinned ISRG
Root X1 + chain verification and hostname check `mqtt.goldwein.net`.

Measurements (log over serial, each at idle and after 10 minutes):

1. Baseline `heap_caps_get_free_size` / min-free (internal, PSRAM, largest block) with Wi-Fi, no MQTT.
2. Idle persistent MQTT/TLS connection, 60 s keepalive: internal and PSRAM free/min-free delta.
3. Same plus the existing MTA HTTPS/TLS poll cycle running concurrently (eight feeds): minimum free internal heap and largest block; confirm no feed timeout/TLS failure regression.
4. Alternative: connect-publish-disconnect per heartbeat; compare heap peak and reconnect latency.
5. Publish QoS 1 status of about 2 KiB and a 4 KiB payload (PUBACK received, collector ingests); oversize 4 KiB+1 rejected client-side by the firmware.
6. Retained status/availability and Last Will retained `offline` (observer: `bkmap-collector` sees them); kill the radio and confirm will fires after keepalive.
7. Reconnect: drop AP, drop broker route (client-side only), expect exponential backoff with jitter capped near 15 min; no reconnect storm; MTA polling unaffected.
8. Certificate verification negative tests: wrong hostname and untrusted CA must fail to connect.
9. Confirm the device receives nothing (no subscriptions) and the ACL blocks cross-topic publishes.
10. Task stack high-water mark and CPU impact of the `esp-mqtt` task.

Pass criteria: sustained minimum free internal heap stays above the design
floor (set from baseline measurement; record the margin), no MTA regressions,
all negative TLS tests fail closed, and QoS 1/will/retain behave as in the live
collector tests. Fallback if `esp-mqtt` exceeds the RAM budget: ArduinoMqttClient
with connect-on-demand. PubSubClient is not preferred (no QoS 1 publish).
