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
  `/ncsi.txt`) receive the portal page directly (HTTP 200); every unknown path
  redirects (302) to `http://192.168.4.1/`. Explicit root, scan and credential
  submission routes are registered before a wildcard HTTP handler. This
  catches unmatched paths/methods before Arduino `WebServer` logs its
  "request handler not found" diagnostic; `onNotFound` remains a fallback.
  Unknown paths redirect to the root; captive probes receive root-page HTML.
  The wildcard does not intercept the explicit `POST /connect` save route.
  Sending HTML on probe requests
  ensures it differs from the expected no-content/success response and gives
  captive assistants the portal content without requiring redirect handling.
  `http://192.168.4.1/` always serves the page directly. This can prompt
  supported phone OSes to offer their captive-portal sign-in view, but
  automatic launch is controlled by the phone OS and cannot be guaranteed
  across all devices and settings. Users must be able to open the setup address
  manually.
- Provisioning starts as AP-only so the setup SSID can beacon before station
  scanning or association begins. The first portal-page request enables
  AP+STA mode to scan nearby networks; submitting credentials uses AP+STA to
  validate the connection while keeping the setup AP available.
- The page shows the heading, "Wi-Fi setup mode", a 2.4 GHz-only notice, a
  de-duplicated scan list (SSID, RSSI, secured/open, strongest first), a
  password field and Save/Connect. There is no manual SSID field; hidden
  networks cannot be provisioned through the portal.
- After a successful LAN association and credential save, the page explicitly
  reports the joined SSID (for example, `Successfully connected to Wi-Fi:
  TestNetwork. Settings saved. LAN address: 192.168.1.42.`). The DHCP-assigned LAN
  address is a clickable `http://<LAN-IP>/` link to the device and is only
  shown once a nonzero address is available. Open it after reconnecting the
  phone to the same Wi-Fi network. SSIDs and addresses are HTML-escaped.
  If the association succeeds but saving credentials fails, the page still
  identifies the joined SSID and LAN IP and clearly reports the storage
  failure. The serial console reports the association/save result and LAN IP;
  passwords are never printed. The
  confirmation includes a link to `http://192.168.4.1/` and explains it only
  works while the setup AP remains available; the AP closes shortly after a
  successful save.
- The 2.4 GHz-only limit of the ESP32-S3 is stated in the UI.
- Scanning is required to choose a network; use Rescan if the list is empty.

### Captive DNS, probes and Android behavior (audit, 2026-10-18)

- Wildcard captive DNS **already existed** and was not changed:
  `DNSServer` in `include/wifi_portal.h`; `Portal::begin` calls
  `dns_.start(53, "*", 192.168.4.1)` after the AP address is set;
  `Portal::service` calls `processNextRequest()`; `Portal::stop` calls
  `dns_.stop()`. It runs only while the setup AP is active; the normal LAN
  server uses a plain 404 for unknown paths and no captive DNS.
- Probe paths (`classifyPath`): `/generate_204`, `/gen_204`,
  `/hotspot-detect.html`, `/library/test/success.html`, `/connecttest.txt`,
  `/ncsi.txt`, `/redirect`, `/fwlink*`. Probes are served the setup page with
  200 (never 204); unknown paths redirect to `http://192.168.4.1/`. Explicit
  routes (`/`, `/scan`, `/connect`, `/admin/setup`) precede the wildcard.
- XIAO hardware result: an arbitrary hostname (`example.com`) resolved to
  `192.168.4.1` and, in the browser, reached the setup page. The Android phone
  requested `http://connectivitycheck.gstatic.com/generate_204` twice per join.
  A temporary experiment answering it with a 302 to `http://192.168.4.1/`
  (plus temporary path logging) made no difference and both were reverted;
  the released Phase 4V firmware source is unchanged. The phone showed
  "connected without internet" and no sign-in prompt, and repeatedly left and
  rejoined the AP. The Android auto-popup was therefore **not** achieved on
  this phone. Physical LED output stayed disabled throughout.
- Not investigated: newer Android versions can also use a DHCP captive-portal
  URL (option 114); no support was found in the installed core headers and it
  was not pursued. Manually opening `http://192.168.4.1/`, or any plain-HTTP
  address, is the supported setup path and always works.

## Save / replace transaction

1. Input is validated (invalid input leaves everything unchanged).
2. The device tries to join the network and obtain a nonzero LAN IP address
   (up to 20 s) **before** saving.
3. Only after Wi-Fi association and IP acquisition is the candidate written to
   NVS (`commitIfAssociated`). On failure, previously stored credentials are
   untouched and the portal stays available for a retry.
4. On success the portal serves the confirmation as HTTP 200 and lingers for
   8 seconds after association, DHCP address acquisition and credential save.
   While connecting, the page requests a refresh every 2 seconds; the 8-second
   post-save linger gives the refreshed success page time to reach/render before
   the AP stops. It then returns to station mode, performs SNTP and resumes
   polling. The AP does not remain active indefinitely after success.
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

`python tools/run_wifi_provisioning_tests.py`: **310 checks, 0 failures**
covering validation (empty, 32-byte max, over-long, password length, hidden),
boot policy, temporary events never erasing/entering setup, replacement
success/failure, forget scope, hold detector, scan de-duplication, portal HTML
(no password, escaping, tiny-buffer safety), path classification, and feed
failure diagnostics (feed name, bytes, elapsed, optional observer).
The Wi-Fi stack itself is not emulated. Build the XIAO bench firmware with
`pio run -e xiao-s3-network`; the default `xiao-s3` build leaves networking off.
For a production image with provisioning and MTA polling enabled, build
`pio run -e brooklyn-s3-network`. The default `brooklyn-s3` environment still
compiles networking out. The network-enabled production target has not been
uploaded to or validated on the manufactured PCB.

The PCB setup flow is deliberately usable without automatic captive-portal
launch: hold the rotary push for 10 seconds, join `BrooklynMap-Avi`, and open
`http://192.168.4.1/` manually if the phone does not offer its sign-in view.
Select a visible 2.4 GHz SSID and enter its Wi-Fi password on the page. Only
visible networks are selectable; hidden SSIDs are not currently supported.

Regression (unchanged): semantic 302/276/26/188 passed; resolver, GTFS adapter,
GTFS-RT, topology (672) passed; Phase 4F 8,098, 4G 5,232, 4H 71,785, 4I
4,538, 4J 1,209 checks, all 0 failures.

## Build sizes

| Build | RAM | Flash |
|---|---|---|
| brooklyn-s3 default | 34,264 B | 306,481 B |
| xiao-s3 default | 33,124 B | 286,529 B |
| brooklyn-s3-network | 69,740 B | 1,025,285 B |
| xiao-s3-network | 68,628 B | 1,006,757 B |

Versus Phase 4K: default brooklyn-s3 +24 B RAM / +360 B flash; default xiao-s3
+8 B RAM / +324 B flash. Captive-portal cost versus the Phase 4K network-enabled
XIAO test build (60,344 B RAM / 955,349 B flash): about +8.3 KB RAM and
+47 KB flash (includes `WebServer`, `DNSServer`, `Preferences`, the portal
page and the new state machine). Network-enabled sizes are from the
2026-10-16 builds; sizes include the joined-SSID, DHCP-IP and captive-path
wildcard handling.

## XIAO hardware results

The initial bring-up notes below are historical; the later dated results
record the subsequent completed provisioning and reboot validation.

### Initial bring-up (historical)

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
- In the initial hardware run, D-G (phone provisioning, NVS persistence across
  reboot, AP shutdown after save, SNTP/polling resume, temporary SSID loss, and
  invalid-credential replacement) were **NOT performed**.

### Subsequent validation
- **2026-10-16 retest:** Added `xiao-s3-network` and changed setup startup to
  start AP-only before enabling station scan/association. On the XIAO, firmware
  reported the AP started and the user confirmed `BrooklynMap-Avi` became
  visible on Android. Android did **not** open its captive-portal view
  automatically; manually opening `http://192.168.4.1/` loaded the setup page.
  The portal scanned 17 networks. The user submitted credentials; association
  succeeded, NVS save succeeded, and the AP closed normally. SNTP synchronized,
  but the first MTA cycle had DNS errors. A PC Wi-Fi scan did not list the AP
  during this retest. Automatic OS launch remains best-effort and is not
  universal.
- **2026-10-16 reboot follow-up:** Opened COM4, rebooting the XIAO. It loaded
  credentials from NVS, synchronized time, and completed an 8/8 MTA feed cycle
  (57 trains, 53 LEDs, 451,239 bytes). This confirms NVS persistence across
  reboot and upstream DNS/feed access; the earlier DNS failure was transient.
- **2026-10-16 invalid replacement:** From the portal, submitted a deliberately
  incorrect password for the saved hotspot SSID. The STA disconnect reported
  reason 15; association failed after the configured 20 s attempt, and the
  portal reported that stored credentials were unchanged. Rebooted through
  COM4 afterward: the device loaded credentials from NVS, synchronized time,
  and completed an 8/8 feed cycle. The previous valid credentials therefore
  survived the failed replacement.
- **2026-10-16 network-loss recovery:** With COM4 open, temporarily turned the
  saved phone hotspot off. The XIAO stayed in station mode, did not start the
  provisioning AP, and retried association at approximately 30 s intervals.
  After the hotspot was restored it synchronized time and completed an 8/8
  feed cycle. One feed cycle during the outage was partial due to Wi-Fi loss.
- **2026-10-16 Android fresh-join retest:** After forgetting and rejoining
  `BrooklynMap-Avi`, Android still did not show a captive sign-in prompt. The
  user can open the portal manually. Updated recognized probe URLs to serve
  portal HTML directly (HTTP 200) instead of a redirect; after flashing this
  build and repeating the fresh join, Android still did not show a prompt, but
  the manual portal URL worked. Automatic launch is phone-OS controlled and
  remains best-effort.
- **2026-10-16 Tailscale-off retest:** With Tailscale disabled, repeated the
  XIAO portal test and joined the AP from Android. The captive sign-in prompt
  still did not appear; manually opening the portal worked. The firmware
  accepted the submitted hotspot credentials, saved them, and closed the AP.
  Tailscale being enabled was therefore not the cause of the missing Android
  prompt in this test.
- **2026-10-16 confirmed LAN provisioning:** User-observed XIAO results:
  stored NVS credentials survived firmware upload/reboot and were used for an
  automatic 8/8 feed cycle before setup mode. Serial `W` entered recovery
  without erasing credentials. `BrooklynMap-Avi` started at
  `192.168.4.1`; a phone joined and the scan returned visible networks.
  Submitted credentials were accepted (only password length logged), the XIAO
  joined the selected LAN, obtained DHCP address `10.110.202.133` for that run,
  and saved the credentials. The AP shut down, SNTP synchronized, and MTA
  polling resumed; cycles 2, 3 and 4 completed 8/8. A preceding DNS failure
  recovered automatically and remains a transient network/Internet failure,
  not a credential failure. Physical LED output remained disabled.
- **Latest portal UI verification:** User visually confirmed the success page
  showed the joined SSID, a clickable DHCP LAN-IP link, a clickable
  `http://192.168.4.1/` setup link, and the note that setup Wi-Fi closes
  shortly. The LAN link is intended for use after the client rejoins the same
  LAN; it is not reachable from the isolated setup AP after that AP stops.
- **Post-provision reset check:** After the updated `xiao-s3-network` image was
  uploaded, the XIAO was reset without sending `W`. The serial capture showed
  two complete 8/8 MTA cycles and repeated `LED outputs DISABLED` heartbeats.
  No setup/provisioning messages appeared in the captured output, consistent
  with automatic use of stored credentials and no setup AP. Startup output
  disconnected during USB reset, so the explicit boot-time NVS-source and SNTP
  lines were not captured in this reset retest. The earlier user-confirmed
  provisioning run did show SNTP synchronization and polling resumption.
- The WebServer diagnostic root cause was verified in the installed Arduino
  WebServer implementation: it logged "request handler not found" before
  invoking `onNotFound` whenever no exact route matched. The portal now
  registers a wildcard `HTTP_ANY` handler after `/`, `/scan`, and `POST
  /connect`, so it redirects otherwise unmatched requests without intercepting
  credential submission. Recognized OS captive probes still serve the portal
  page directly. Host tests and both network builds pass after this change;
  production PCB has not been physically tested.

## Not validated

Production PCB (rotary hold, GPIO17), a physical-browser request to an
arbitrary unknown URL (the wildcard is host-tested, built and flashed, but
this specific browser test was not captured), the phone-side provisioning
transaction on iOS/Windows, Android automatic captive-portal launch,
production PCB recovery, and flash/NVS encryption.
