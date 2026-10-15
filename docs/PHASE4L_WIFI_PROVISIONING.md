# Phase 4L: Wi-Fi provisioning and captive portal

Baseline: Phase 4K (`c85f083`, tag `phase-4k-hardware-network-bringup`).
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` for both targets; this phase never
touches LEDs, LED_ENABLE or LED GPIOs. Production PCB behavior is NOT
validated (the board is not available); all hardware results below are from
the XIAO ESP32-S3 prototype.

## Architecture

| Unit | Role |
|---|---|
| `include/wifi_policy.h`, `src/wifi_policy.cpp` | Pure, Arduino-free, host-tested: credential validation, boot/recovery policy, replacement transaction, 10 s hold detector, scan de-duplication, portal HTML renderer, path classification |
| `include/wifi_portal.h`, `src/wifi_portal.cpp` | ESP32 adapters: NVS credential store, SoftAP + wildcard DNS + `WebServer` portal (Arduino-ESP32 built-ins only; no new dependency). Compiled only with `BROOKLYN_NETWORK_ENABLED=1` |
| `src/network_service.cpp` | State machine integration (provisioning vs station); `feed_transport` is untouched by provisioning logic |
| `src/controls.cpp` | Production 10 s `ROTARY_PUSH` hold -> setup-mode request |
| `src/main.cpp` | XIAO serial commands |

States: `Disabled`, `NoCredentials`, `Provisioning`, `WifiConnecting`,
`TemporarilyUnavailable` (credentials exist, SSID not visible), `TimeSyncing`,
`Ready`.

## Credential storage

NVS namespace `bkmap_wifi` (map-owned), keys `ver` (record version 1), `ssid`,
`pass`. A record is valid only if `ver == 1` and the SSID/password pass
validation (SSID 1-32 bytes; password empty = open network, otherwise 8-63).
Hidden SSIDs are not selectable in the portal (no manual-entry field). `clear()` empties only this namespace;
other NVS data is never erased. The driver is told not to persist its own copy
(`WiFi.persistent(false)`).

The password is never printed, never rendered into HTML (the page has an empty
password field and renders no stored value), and never logged. The portal logs
only the SSID *length*.

**Security note:** ordinary ESP32 NVS is not hardware-encrypted. The password
is stored in plaintext in flash unless flash/NVS encryption is separately
configured (not done in this phase). Physical access to the board's flash
exposes it.

## Role of `include/secrets.h`

Optional, git-ignored developer fallback, used **only** when NVS has no valid
record. It is never copied into NVS and never overrides stored credentials.
It is never required and never printed. Priority: NVS credentials, then
(developer-only) `secrets.h`, otherwise provisioning.

## Boot behavior

- **No credentials anywhere:** start provisioning immediately (no waiting on an
  absent network); the MTA poller does not run.
- **Stored credentials (or developer fallback):** normal station mode with the
  Phase 4K retry behavior (30 s retry spacing). `WL_NO_SSID_AVAIL`,
  failed association, Wi-Fi loss, feed timeouts, DNS/Internet failure and
  TLS failure never erase credentials and never enter provisioning. They map
  to `TemporarilyUnavailable`/transport counters only.
- Only an explicit user action enters setup mode (`W` on XIAO, 10 s rotary
  hold on production). Only an explicit confirmed forget erases credentials.

## Setup AP and portal

- SSID `BrooklynMap-Avi`, address `192.168.4.1`, **open** (no password).
  Reasoning: a shared hard-coded password would be public to every owner of
  this firmware and gives false security; an open AP is the common practice for
  setup portals. Exposure is limited to the setup window and to someone within
  radio range; the stored home-Wi-Fi password travels over this unencrypted
  link when submitted. A per-device password/WPA2 setup AP (e.g. printed on a
  label) is a possible future hardening.
- Wildcard DNS answers every name with 192.168.4.1. Probe URLs (Android
  `/generate_204`, iOS `/hotspot-detect.html`, Windows `/connecttest.txt`
  `/ncsi.txt`) and every unknown path redirect (302) to `http://192.168.4.1/`.
  `http://192.168.4.1/` always serves the page directly.
- The page shows the heading, "Wi-Fi setup mode", a 2.4 GHz-only notice, a
  de-duplicated scan list (SSID, RSSI, secured/open, strongest first), a
  password field and Save/Connect. There is no manual SSID field; hidden
  networks cannot be provisioned through the portal.
- The 2.4 GHz-only limit of the ESP32-S3 is stated in the UI.
- Scanning is required to choose a network; use Rescan if the list is empty.

## Save / replace transaction

1. Input is validated (invalid input leaves everything unchanged).
2. The device tries to associate with the candidate (20 s) **before** saving.
3. Only on association success is the candidate written to NVS
   (`commitIfAssociated`). On failure, previously stored credentials are
   untouched and the portal stays available for a retry.
4. On success the portal lingers 3 s (so the "saved" page reaches the phone),
   stops the AP, returns to station mode, performs SNTP and resumes polling.
   With no old credentials the portal simply stays until valid ones work. With
   stored credentials, an unused portal times out after 10 minutes and the
   device resumes with them.

## Recovery triggers

- **XIAO (bench only):** serial `W` enters setup mode (no firmware re-upload,
  LEDs stay disabled). `X` then `X` again within 5 s forgets the stored Wi-Fi
  credentials (deliberate two-step confirmation). `T`/`M` unchanged.
- **Production intent:** continuous ~10 s hold of `ROTARY_PUSH` (GPIO17, the
  assignment already in `include/board_pins.h`) requests setup mode. The
  detector (`HoldDetector`) is host-tested, including `millis()` wraparound,
  fires once per hold and re-arms on release. It does not erase credentials.
  **Status:** implemented but NOT validated on the production PCB (unavailable).

## Hotel / upstream captive-portal limitation

The `BrooklynMap-Avi` portal only provisions the map's Wi-Fi credentials. It
does not complete a hotel/airport/public provider's separate browser-based
login (room number, surname, terms, voucher). Association with such an SSID can
succeed while Internet/MTA access stays blocked; that is a transport failure,
not a credential failure, and never erases credentials. Do not expect the map
to automate those portals. Use a phone hotspot, Windows Mobile Hotspot or a
travel router to provide ordinary Internet access instead.

## Feed-specific timeout diagnostics (diagnostic only)

On any failed feed the cycle now prints one line, e.g.
`Feed ACE FAIL: timeout bytes=300 elapsed=8150ms`
(short name, failure category, body bytes received before failure, elapsed
request/download ms). No body content is logged. The observer is optional and
does not change counters or control flow. **Timeout policy is unchanged:**
connect 8 s, read-idle 8 s, body deadline 12 s, cycle budget 60 s.

## Host tests

`python tools/run_wifi_provisioning_tests.py`: **273 checks, 0 failures**
covering validation (empty, 32-byte max, over-long, password length, hidden),
boot policy, temporary events never erasing/entering setup, replacement
success/failure, forget scope, hold detector, scan de-duplication, portal HTML
(no password, escaping, tiny-buffer safety), path classification, and feed
failure diagnostics (feed name, bytes, elapsed, optional observer).
The Wi-Fi stack itself is not emulated.

Regression (unchanged): semantic 302/276/26/188 passed; resolver, GTFS adapter,
GTFS-RT, topology (672) passed; Phase 4F 8,098, 4G 5,232, 4H 71,785, 4I
4,538, 4J 1,209 checks, all 0 failures.

## Build sizes

| Build | RAM | Flash |
|---|---|---|
| brooklyn-s3 default | 34,264 B | 306,481 B |
| xiao-s3 default | 33,124 B | 286,529 B |
| brooklyn-s3 network-enabled | 69,712 B | 1,021,021 B |
| xiao-s3 network-enabled | 68,600 B | 1,002,449 B |

Versus Phase 4K: default brooklyn-s3 +24 B RAM / +360 B flash; default xiao-s3
+8 B RAM / +324 B flash. Captive-portal cost versus the Phase 4K network-enabled
XIAO test build (60,344 B RAM / 955,349 B flash): about +8.3 KB RAM and
+47 KB flash (includes `WebServer`, `DNSServer`, `Preferences`, the portal
page and the new state machine).

## XIAO hardware results (this session; the user left before the phone steps)

Verified on the physical XIAO (network-enabled test build, `secrets.h`
developer fallback, no NVS record yet):

- A. Existing-credential path: connected, SNTP synced, 8/8-feed COMPLETE cycle
  (141 trains, 114 LEDs). **Passed.**
- B. Serial `W` entered setup mode without a reboot: AP `BrooklynMap-Avi` at
  192.168.4.1 started (stored credentials kept). **Passed.**
- C. A phone joined the AP. First attempt: joined but `http://192.168.4.1/` timed
  out; fixed by starting the AP before applying the AP IP config. Second: the
  page loaded; the scan list was empty (`WiFi.scanComplete()` = -2, scan
  failed while the radio was in AP+STA). Added a short settling delay and
  scan retry; after that the board logged `scan found 9 network(s)`. The user
  then reported the AP visible and the page reachable, but left before
  confirming the rendered list. **The rendered scan list and the
  "password not exposed" check on the phone are therefore NOT confirmed.**
- D-G (provision a test network from the phone, NVS persistence across reboot,
  AP shutdown after save, SNTP/polling resume after provisioning, temporary
  SSID loss with credentials kept, invalid-credential replacement) were
  **NOT performed**. They remain to be run.

## Not validated

Production PCB (rotary hold, GPIO17), the phone-side provisioning transaction
end to end, NVS persistence/reboot, OS captive-portal pop-ups on
Android/iOS/Windows (only direct `http://192.168.4.1/` was exercised), and
flash/NVS encryption.
