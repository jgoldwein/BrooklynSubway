# Phase 4R — normal-mode LAN configuration UI

## Scope and safety

Phase 4R adds a local status and configuration interface for an already
provisioned map. The interface is active only while station Wi-Fi is connected.
It uses the existing Arduino `WebServer` implementation on port 80 and the
ESP32 Arduino `ESPmDNS` library; no package or manufacturing dependency was
added.

The provisioning portal and normal LAN interface share port 80 but never run
at the same time. Entering setup mode stops the LAN server and mDNS before the
existing `BrooklynMap-Avi` portal starts. When provisioning ends and station
association is restored, the LAN interface can start again.

The portal guards handler registration across stop/start cycles. Arduino
`WebServer::on()` appends a heap-allocated handler each time it is called and
does not replace or deduplicate existing handlers. Repeated Change-Wi-Fi
handoffs would otherwise accumulate duplicate portal routes. Normal-LAN
handlers are registered once and retained over server stop/start.

The `xiao-s3-network` image was uploaded to the authorized XIAO bench target on
COM4 during the Phase 4R hardware validation attempt. Physical LED output
remains disabled. Every PlatformIO environment retains
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. The display brightness control changes
only the logical RGB frame brightness; it does not enable or call a physical
LED output path. The production PCB remains physically untested.

## Access and endpoints

Open `http://<current-lan-ip>/` from a client on the same trusted LAN as the
map. The current DHCP address is shown as a clickable link. If mDNS starts
successfully, `http://brooklynmap.local/` is also shown; the numeric IP remains
the fallback. mDNS failure is reported over serial but does not disable the
HTTP interface.

The page refreshes every 20 seconds. `GET /api/status` returns the same
operational information as JSON. The interface includes:

- firmware phase, target, uptime, and reset reason
- station connection, SSID, RSSI, LAN IP, hostname, mDNS availability, and
  time synchronization
- latest feed-cycle outcome and successful/attempted feed counts
- operational display state, age of the last usable and complete updates,
  resolved train count, occupied LED count, and collisions
- internal heap and PSRAM availability/low-water diagnostics, plus whether
  the feed buffer is in PSRAM
- current logical brightness and an explicit physical-output-disabled status

State-changing requests use POST:

- `POST /config/brightness` accepts a decimal `brightness` value from 0 through
  255 and returns a redirect to `/` after a successful save.
- `GET` or `POST /wifi/setup` displays a confirmation page.
  `POST /wifi/setup/confirm` stops the LAN service and requests the existing
  Wi-Fi setup portal. Existing credentials are retained unless replacement
  credentials successfully connect.
- `POST /reboot` returns a reboot notice, then reboots after a short response
  delay. Wi-Fi credentials and normal configuration are retained.

Unknown normal-mode paths and unsupported methods return a plain 404 through
the normal server's `onNotFound` handler. This does not redirect to the setup
AP; provisioning-mode unknown/probe requests keep their separate Phase 4L
redirect behavior. The brightness slider only updates the companion number
input while dragged. NVS is written once, only when the user submits the Save
brightness form. The WebServer API exposes POST body values but not their
argument names/counts; duplicate `brightness` body fields therefore use the
framework's first matching value and are not independently rejected.

## Brightness persistence and credentials

Brightness is stored in NVS namespace `bkmap_cfg`, key `brightness`. This is
separate from the existing `bkmap_wifi` credential storage. The default is
64/255. Only values from 0 through 255 are accepted. Missing or out-of-range
stored values use the default; an NVS write failure returns HTTP 500 and does
not change the in-memory setting.

The LAN UI has no password or Wi-Fi credential field. It does not render or
log a Wi-Fi password. Existing network failure semantics are unchanged:
temporary SSID loss and DNS, Internet, TLS, or MTA failures do not clear or
replace stored credentials and do not start provisioning.

The interface has no authentication or TLS. It is intended only for a
trusted, private local network; do not expose port 80 through a router,
firewall rule, or public network. POST-only mutations avoid changing state on
ordinary page loads but are not an authentication or CSRF boundary.

Wi-Fi credentials remain isolated in the existing `bkmap_wifi` namespace
(`ver`, `ssid`, and `pass`). Phase 4R reads/writes only `bkmap_cfg` key
`brightness`. It does not call `Preferences.clear()` or clear any namespace.
The reboot action sends its response and then calls `ESP.restart()`; it does
not clear NVS or request provisioning. The Change Wi-Fi action calls the
existing `NetworkService::startProvisioning()` path; stored credentials are
retained, and the Phase 4L replacement transaction saves only after a
successful association. The normal server is stopped before portal startup,
and portal shutdown completes before normal LAN startup.

## Memory and build results

The firmware emits the target object sizes at network-enabled startup. The
clean XIAO ESP32-S3 network build's target debug information reports:

- `DeviceConfiguration`: 16 bytes
- `LanWebUi`: 508 bytes, including the `WebServer` object and its status
  snapshot
- the repeated-portal-route guard is one additional byte in the persistent
  `Portal` instance, subject to object alignment

The page and JSON response buffers are fixed static allocations of 6,144 and
2,048 bytes respectively. A representative host-rendered status snapshot
generated a 3,287-byte home page and a 660-byte status JSON response; actual
lengths vary with dynamic SSID/device/status text.
The measured 10,856-byte network-build RAM increase is primarily the linked
Wi-Fi/TLS/HTTP/mDNS framework state and related network configuration; the two
reported Phase 4R objects total 524 bytes, with the fixed response buffers
accounting for 8,192 bytes of static storage. These are useful component
measurements, not an exact linker-section attribution.

SSID and other dynamic HTML text are HTML escaped, including quotes,
apostrophes, ampersands, and angle brackets; ASCII control bytes are rendered
as numeric entities. The LAN-IP link is emitted only after strict IPv4
validation; form action attributes and hostnames are fixed constants. The JSON
writer independently escapes quotes, backslashes, standard whitespace
controls, and other bytes below 0x20. It emits no password property. The HTML
is self-contained (inline CSS, no external script, font, image, or CDN),
includes a mobile viewport, labels the brightness inputs, displays the current
value, and presents separate Change Wi-Fi and confirmation-gated Reboot
controls. Physical-output state is informational only.

The status page is rendered from one loop-owned snapshot. Network identity
(SSID/IP) is captured when the station LAN server starts, rather than
allocating new Arduino `String` values every main-loop iteration. RSSI,
time-sync state, uptime, and heap metrics refresh each loop. Display state,
ages, and train/LED counts come from the Phase 4M operational-display
controller; unavailable ages are rendered as “Not available” in HTML and
`null` in JSON. Its age calculation uses unsigned `millis()` deltas, with
rollover behavior covered by Phase 4M host tests; the web layer adds no second
stale policy. Feed outcome and success counts come from the most recent
transport cycle.

Clean PlatformIO builds passed:

| Environment | RAM | Flash |
|---|---:|---:|
| `brooklyn-s3` | 36,236 B | 304,429 B |
| `xiao-s3` | 35,348 B | 289,741 B |
| `brooklyn-s3-network` | 82,596 B | 1,064,457 B |
| `xiao-s3-network` | 81,708 B | 1,049,889 B |

Compared with the Phase 4Q checkpoint builds, the RAM/flash deltas are:

| Environment | RAM delta | Flash delta |
|---|---:|---:|
| `brooklyn-s3` | 0 B | +120 B |
| `xiao-s3` | 0 B | +156 B |
| `brooklyn-s3-network` | +10,856 B | +39,888 B |
| `xiao-s3-network` | +10,856 B | +40,024 B |

The production `brooklyn-s3-network` ELF was built with physical-output
permission zero. It contains no project physical-strip construction,
physical transmit callback, `Adafruit_NeoPixel::show`, or
`brooklyn::led::safety::Controller` symbols. The diagnostic-only query
functions `physicalLedBackendInitialized()` and
`physicalLedOutputPermissionEnabled()` remain linked. Arduino core's generic
`neopixelWrite` helper is also linked through `esp32-hal-gpio.c`; the firmware
has no direct `neopixelWrite` call, and the production LED data pins are not
the core's built-in RGB LED pin. This framework symbol does not indicate that
the project's physical-strip path is linked or enabled.

## XIAO hardware validation attempt — 2026-10-17

The clean `xiao-s3-network` image was uploaded to the authorized XIAO on COM4.
Esptool verified the image write; the uploaded `firmware.bin` SHA-256 was
`6A2732A5B5A63A51DB274CC63710B2F7398A8C9D4C22E1719799B147ACEB5B3A`
(1,050,144 bytes). No production target was uploaded.

Serial startup confirmed the XIAO target, stored Wi-Fi credentials loaded from
NVS, logical brightness defaulting to 64 because the `bkmap_cfg` brightness
namespace had no saved value, a 512 KiB feed buffer allocated in PSRAM, and
physical LED outputs disabled. Heartbeats continued without watchdog/reset
diagnostics, and no password or credential value was printed. No physical
LEDs were connected.

Hardware LAN validation is paused because the configured 2.4 GHz SSID is
unavailable at Joel's present location, not because of an observed Phase 4R
firmware defect. The board repeatedly attempted station association; the
observed Wi-Fi status value 1 is `WL_NO_SSID_AVAIL` in the installed Arduino
Wi-Fi definitions. Stored credentials were retained and the firmware did not
enter provisioning. No DHCP address, SNTP synchronization, normal LAN HTTP
server, mDNS startup, or MTA polling cycle was observed. The development PC
could not resolve `brooklynmap.local` and had no neighbor-cache entry for the
XIAO.

Because station association did not complete, the live-device HTTP/API and
HTML checks, brightness POST/NVS persistence, reboot endpoint test, Change
Wi-Fi handoff, same-LAN port-80 transition, and web/MTA coexistence tests could
not be exercised. Brightness remained at its default of 64; no test value was
written. The serial output and XIAO ELF both confirmed physical output
disabled; the Phase 4Q physical-output controller was not linked in the XIAO
image.

Startup memory observations: 8 MB PSRAM detected; internal free heap was
300,360 bytes before feed-buffer allocation and remained 300,360 bytes after;
PSRAM free memory fell from 8,386,295 to 7,861,991 bytes after allocation.
No HTTP traffic occurred, so post-request heap stability was not measured.

The hardware validation is **paused and is not a Phase 4R checkpoint**.
Repeat the LAN-facing tests when the stored 2.4 GHz network is reachable and
the development PC is connected to that same LAN. No replacement credentials
were submitted. The local ignored `include/secrets.h` remains untracked; a
credential-value scan found no stored password or SSID in tracked or untracked
repository files. A real SSID found in an existing host fixture and
provisioning-document example was replaced with the generic `TestNetwork`
label.

## Follow-up hardware retry — 2026-10-17

The development PC remained on its existing Wi-Fi network; its address was
`10.111.14.47/23` with gateway `10.111.15.254`. No Windows Mobile Hotspot was
created. While monitoring COM4, the authorized `W` recovery command was sent.
Serial reported that setup mode was requested with stored credentials kept,
and confirmed `BrooklynMap-Avi` at `192.168.4.1`. The user submitted the
current Wi-Fi credentials directly through the portal; no password was
requested in chat or captured in the serial transcript.

A subsequent COM4 capture showed a `COMPLETE` MTA cycle with all eight feeds
processed, 69 trains, 61 occupied LEDs, seven collisions, and zero feed
decode failures. At cycle 79 the counters showed zero Wi-Fi-unavailable and
time-unavailable events; this functionally demonstrates that station
connectivity and valid time were available for successful polling. A serial
heartbeat continued to report `LED outputs DISABLED`. The cycle-79 memory
report was PSRAM free 7,846,323 B (minimum 7,775,143 B), internal free heap
247,760 B (minimum 195,364 B, largest block 188,404 B). These are post-feed
observations, not post-HTTP memory measurements.

The serial capture did not include explicit association, DHCP-address,
mDNS-startup, or SNTP-success messages. The XIAO's DHCP IP is therefore not
known from this run. The PC could not resolve `brooklynmap.local`, and a
multicast DNS A query returned no answer. A port-80 search limited to the
PC's directly connected `10.111.14.0/23` subnet found no Brooklyn LAN UI
response. Since the exact XIAO address was not obtained, direct device
reachability and client isolation are **undetermined**; this is not evidence
of a firmware defect. A captive/upstream network policy or client isolation
remains possible.

No browser page/API request, brightness change, reboot/persistence test,
Change-Wi-Fi handoff, HTTP/MTA coexistence test, or post-request memory
observation was completed. Logical brightness remained at 64. The user will
retry later on the existing Wi-Fi network; do not create a Mobile Hotspot.
Resume by obtaining the DHCP IP (including from the portal success page if
available), testing direct PC-to-XIAO reachability, and continuing the
remaining Phase 4R checks. No firmware upload or physical LED activity
occurred during this retry.

## Direct reachability retry — 2026-10-17

The user indicated Tailscale was turned off. The PC remained at
`10.111.14.47/23` with gateway `10.111.15.254`. Windows routed the candidate
XIAO address directly over Wi-Fi. The USB serial hardware identifier is
`94-A9-90-D0-94-54`; Windows learned the same MAC at `10.111.15.91`, making
`10.111.15.91` the likely current DHCP address, though no serial DHCP line or
portal page captured it explicitly.

From the PC, three ICMP requests to `10.111.15.91` timed out. TCP connections
to ports 80 and 5353 timed out, and a repeated mDNS query for
`brooklynmap.local` received no packet. The LAN UI therefore could not be
tested. The route was through the correct Wi-Fi interface, but the Tailscale
virtual adapter still appeared as Up in Windows despite the user's report
that Tailscale had been turned off. Wireless/client isolation or other
network access policy is possible but **not confirmed**; no access-point
configuration evidence was available. Do not classify this as a Phase 4R
firmware defect.

No firmware, credential, or brightness changes were made. Brightness remains
64 and serial heartbeats have reported LED outputs disabled. Continue only
after client-to-client access on the existing LAN is confirmed or a suitable
non-isolated LAN is available; then run the pending live UI/API and lifecycle
checks.

## PC Mobile Hotspot hardware validation — 2026-10-17

Joel manually confirmed the Phase 4R page rendered correctly in a browser.
The PC hosted the test hotspot at `192.168.137.1/24`; the XIAO first received
`192.168.137.31`, then after reboot DHCP assigned `192.168.137.25`. Windows
matched `.25` to the XIAO USB MAC `94-A9-90-D0-94-54`. The current XIAO address
is `192.168.137.25`. The PC stayed on its working network/hotspot throughout
the Change-Wi-Fi test.

### LAN page, API, and mDNS

At the current DHCP IP, `GET /` returned HTTP 200 and
`text/html; charset=utf-8` (3,305-3,316 bytes in samples). It contained the
Phase 4R Brooklyn Active Subway Map page, device/target identity, network and
operational status, brightness, and Change Wi-Fi/Reboot controls. Numeric IP
and mDNS links were present. No password field/value appeared, and the page
had no external font/CDN/script dependency. Joel visually confirmed the
rendered page, including LED output disabled, connected state, IP/RSSI,
hostname, synchronized time, feed/display diagnostics, and brightness 64.
In the user's browser screenshot, operational display state was `StaleHold`
and the latest feed cycle was `NO FEEDS` 0/8 while the previous successful
update age was still tracked; a single temporary feed failure is not itself
a Phase 4R defect.

`GET /api/status` returned HTTP 200 and valid JSON (674-681 bytes). A sample
after reconnect reported: uptime 14,473 s; connected true; IP
`192.168.137.31`; RSSI -37; mDNS true; time synchronized true; display Fresh;
feed cycle COMPLETE, 8/8; 76 resolved trains; 69 occupied LEDs; 7 collisions;
brightness 64; physical output false; internal free heap 242,292 B
(minimum 191,744 B); PSRAM free 7,844,327 B (minimum 7,768,055 B). Later,
after feed DNS failures aged out usable data, the display correctly advanced
through StaleHold/NoData and the API reported NO FEEDS 0/8 without losing the
LAN connection or changing credentials.

Serial reported `LAN UI: mDNS available at http://brooklynmap.local/`.
Windows resolved `brooklynmap.local` to the current `.25` address, and both
`http://brooklynmap.local/` and `/api/status` returned HTTP 200.

### Brightness and reboot persistence

Initial brightness was 64. POST `/config/brightness` with 73 returned the
expected 303 redirect; API and rendered page both showed 73. The reboot POST
returned a reboot notice and caused a reboot. Serial then showed NVS as the
credential source, brightness 73 loaded from `bkmap_cfg`, Wi-Fi connection,
mDNS and HTTP startup, SNTP synchronization, and a COMPLETE 8/8 MTA cycle.
The LAN API returned at `192.168.137.31` with brightness 73 and output false.

Brightness was restored with POST value 64 (303 redirect); both page and API
showed 64. A second reboot again loaded brightness 64 from NVS, rejoined
Wi-Fi, started mDNS/HTTP, synchronized time, and completed an 8/8 cycle.
The device was left at brightness 64. DHCP was not static: its address later
changed to `.25`, which was rediscovered and used for subsequent tests.

### Method safety, HTTP load, MTA, and memory

GET `/phase4r-unknown-test` returned 404 without redirect. GET `/reboot`
also returned 404 and did not reboot; uptime continued. No handler-not-found,
duplicate-handler, or port-ownership errors were observed.

During a 105-second run, 279 GET `/` and 279 GET `/api/status` requests
succeeded with zero HTTP or state errors. A second concurrent 110-second
run completed 70 requests to each endpoint, again with zero errors, while
four polling cycles ran. The host remained connected and brightness stayed
64; physical output remained false/disabled.

One poll after reboot was COMPLETE 8/8. Subsequent cycles reported NO FEEDS
0/8. Serial showed `hostByName(): DNS Failed for api-endpoint.mta.info` for
each feed, 8-9 ms per feed, zero downloaded bytes, Wi-Fi unavailable 0, and
time unavailable 0. Four repeated NO FEEDS cycles occurred during the
concurrent HTTP run. At the same time, the PC resolved the MTA hostname and
retrieved the public feed over normal certificate-validated HTTPS with HTTP
200. This indicates a DNS/upstream resolution issue on the XIAO's hosted
hotspot path rather than a credential, NVS, or firmware-provisioning fault.
SNTP remained synchronized; the XIAO stayed connected, LAN HTTP remained
responsive, credentials were retained, and no provisioning started. No
transport policy, timeout, or TLS setting was changed. The network-specific
DNS failure/recovery semantics remain intact.

Memory during repeated HTTP and feed attempts:

- API free internal heap ranged around 245,948-247,536 B during the HTTP
  loops; a later serial sequence ranged 247,160-247,700 B.
- Serial internal minimum was 234,292 B after repeated HTTP plus four poll
  attempts; largest reported free block was 237,556 B.
- Free PSRAM stayed about 7,844,479-7,846,807 B during the last test;
  minimum was 7,835,167 B.

The observed allocator variation and one-time low-water movement did not show
a sustained leak. No watchdog, Guru Meditation, spontaneous reboot, or loss
of HTTP service was observed. Heartbeats consistently said LED outputs
DISABLED.

### Change-Wi-Fi handoff and secret/safety audit

One normal-mode handoff was tested. GET/POST `/wifi/setup` only showed the
confirmation page. POST `/wifi/setup/confirm` returned the setup notice;
serial confirmed the LAN HTTP server stopped before
`BrooklynMap-Avi` started at `192.168.4.1`. No replacement credentials were
submitted. An initial RTS-only reset pulse did not restart the XIAO; a
reset-only esptool USB-JTAG `chip_id` operation (no flash write) then reset
the unit. The XIAO booted from the saved NVS credentials and returned
automatically to the PC hotspot at `.25`; API reported brightness 64, mDNS,
network connected, and output false. The PC remained on its LAN throughout.

The local-secret scan checked the ignored `include/secrets.h` values without
printing them; no match was found in tracked or non-ignored untracked files.
No password was present in browser HTML, JSON, brightness/reboot/setup
responses, or captured serial. Every observed heartbeat/API state reported
physical LED output disabled. The XIAO was the only hardware used; production
PCB remains physically untested.

## Host validation

All thirteen host regression runners passed after the final host-hardening
changes. The LAN UI policy/configuration runner passed **98 checks, 0
failures**. Coverage includes brightness parsing and persistence, defaults and
corrupt values, safe HTML/JSON escaping (including control characters),
dynamic IP links, optional mDNS, secret absence, output-disabled status,
bounded rendering, displayed-snapshot counts, and exclusive provisioning/LAN
server ownership transitions.

The Wi-Fi provisioning runner passed **313 checks, 0 failures**. The remaining
regression results were:

- train pipeline: 4,538
- TrainFrame: 5,232
- RGB renderer: 71,785
- resolver: 958,614
- previous stop: 8,098
- physical output safety: 3,630
- operational display: 1,534
- GTFS-RT: 429
- GTFS adapter: 7,383
- feed transport: 1,209
- display output bridge: 476,271

The host regression total is **1,539,134 checks, 0 failures**.

Semantic validation in those regressions confirms 302 map LEDs (276 STATION,
26 SEGMENT), 188 registry entities, and complete resolver/adapter coverage.

Live browser interaction, LAN HTTP responses, and the XIAO network lifecycle
were captured above. No physical LED write or production-PCB test was
performed. The manufactured PCB remains unvalidated.
