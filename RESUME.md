# Resuming work after a reboot

Use this file when returning after a planned or unexpected computer, VS Code, or
agent-session restart. The checkpoint below is a handoff, not a substitute for
checking the live repository and hardware state.

## Resume procedure

1. Open the repository root and read `AGENTS.md` and `PROJECT_CONTEXT.md` before
   changing firmware. Then read this file and the task-specific document linked
   in the current checkpoint.
2. Inspect `git status`, the current branch and latest commit, and review
   `git diff` (including untracked files). Treat every existing change as user
   work: preserve it, do not reset, check out, clean, or overwrite it. Compare
   the live state with the checkpoint below; the repository is authoritative if
   they differ.
3. Check which board is actually connected and whether it is powered before
   uploading, resetting, or running hardware tests. The XIAO is a bench target,
   not a substitute for the manufactured PCB. Never flash the production target
   based on an assumption. Ask Joel if the target or safe next step is unclear.
4. Continue from the checkpoint's next actions. Re-run only the targeted
   validation needed for the change, and report any test that could not be run.
5. Before ending a work session or intentionally rebooting, update the
   checkpoint below: record the date, task/status, relevant commit, changed
   files, validation results, hardware/firmware state, and the next concrete
   action. Do not record passwords, tokens, or other secrets. Keep the history
   concise and replace stale status rather than accumulating contradictory
   instructions.

If the chat history is unavailable, do not guess what the user intended from
the changed files alone. Use this checkpoint and the git history; ask Joel for
clarification only if the requested next action remains ambiguous.

## Current checkpoint — Phase 4V LAN access protection validated on XIAO

- **Checkpoint date:** 2026-10-18. Starting baseline was clean `main` at
  `10095a40ad23aa4e1b15dbb728b6f3a2bf127d17`, tagged
  `phase-4u-dns-resilience`.
- **Release restriction:** Phase 4V has been uploaded to the authorized XIAO
  bench target only. Do not upload the manufactured-board environment.
- **Implementation:** public read-only status remains available. All normal
  LAN mutation endpoints require an administrator session and CSRF, including
  login's separate one-use challenge. First-time administrator setup and
  password reset are only exposed in the separate local `BrooklynMap-Avi`
  provisioning portal. Password verifiers use salted PBKDF2-HMAC-SHA256;
  session pool, expiry, lockout, cookies, and narrowly scoped NVS persistence
  are documented in `docs/PHASE4V_LAN_ACCESS_PROTECTION.md`.
- **Host validation:** all 16 host runners passed **1,554,395 checks, 0
  failures**.
  Focused results: administrator auth 120, LAN policy/rendering 303, and Wi-Fi
  provisioning 332 checks, all with zero failures. `git diff --check` passed.
- **Clean builds:** all four PlatformIO environments compiled after clean:
  `xiao-s3` 35,380 B RAM / 290,189 B flash; `brooklyn-s3` 36,268 B /
  304,873 B; `xiao-s3-network` 92,388 B / 1,089,053 B; and
  `brooklyn-s3-network` 93,276 B / 1,105,709 B. All environments retain
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. Both network ELFs were inspected;
  no project physical-output, strip-transmit, `Adafruit_NeoPixel::show()`, or
  safety-controller symbols were present.
- **XIAO live validation:** verified COM4 USB serial MAC
  `94:A9:90:D0:94:54`; uploaded only `xiao-s3-network`. Esptool reported
  successful flash writes with each data hash verified; the device then
  reported Phase 4V at `192.168.137.25`. Before upload, Phase 4U state was
  brightness 64, all 18 routes visible (`262143`), manual DNS disabled, and
  physical LED output disabled. After upload these values remained unchanged;
  Wi-Fi reconnected and `adminConfigured` was false. Public page/status/
  diagnostics returned 200. Unconfigured login and every tested anonymous
  LAN mutation returned 403; `/admin/setup` on the LAN returned 404. No
  configuration changed. A public-response scan found no credential, CSRF,
  verifier, or session-token fields. Forty repeated status reads passed;
  free internal heap changed from 235,856 B to 235,096 B and PSRAM stayed at
  7,844,479 B. No administrator password has been entered or recorded.
- **Live admin checks (passed):** first-time setup via the local AP, locked
  state, wrong-password 401, 429 lockout, user login, CSRF denial and success,
  logout, reboot persistence, `W` setup re-entry, and a secret-leak scan all
  passed on the XIAO. Details and deferred items (two-session limit, repeated
  cycles, AP password reset submit, heap tracking) are in
  `docs/PHASE4V_LAN_ACCESS_PROTECTION.md`. The final image (success page shows
  only the result) was flashed to the XIAO only; its network ELF audits are
  clean (final sizes: `xiao-s3-network` 1,089,245 B flash, `brooklyn-s3-network`
  1,105,901 B).
- **Hardware/network safety:** only the XIAO was flashed; the manufactured
  PCB was not accessed. No host network, hotspot, or Tailscale settings were
  changed. Physical LED output remains disabled.
- **Changed files:** `include/admin_auth.h`, `src/admin_auth.cpp`,
  `src/admin_auth_esp.cpp`, LAN web policy/UI headers and sources, Wi-Fi
  portal/policy headers and sources, focused host tests and runner, this
  checkpoint, and `docs/PHASE4V_LAN_ACCESS_PROTECTION.md`.
- **Next actions:** Phase 4V is released (tag
  `phase-4v-lan-access-protection`). Do not start Phase 4W without direction.

## Phase 4U checkpoint — DNS resilience validated (historical)

- **Checkpoint date:** 2026-10-18. Phase 4U started with `main` at
  `67221f3` (`docs: record Phase 4T release checkpoint`), after the Phase 4S
  and Phase 4T releases below. Its validated implementation commit is
  `e8a7aba` (`Phase 4U: add optional manual DNS override`).
- **Phase 4U release marker:** annotated tag `phase-4u-dns-resilience`
  identifies the validated Phase 4U release including this final handoff.
  Verify the tag and `main` remote refs if resuming after an interrupted
  release push.
- **Phase 4S / 4T history:** Phase 4S is commit `e39bfbd9eefeeb4c02ee2bda36d694b2bcde4ae4`,
  tagged `phase-4s-route-display-preferences`. Phase 4T is commit
  `a5e8da19b567e5956c53770ad1e3b0ba8bf83944`, tagged
  `phase-4t-network-diagnostics`. Both releases and tags were pushed to
  `origin`; Phase 4T diagnostics are documented in
  `docs/PHASE4T_NETWORK_DIAGNOSTICS.md`.
- **Phase 4U implementation:** direct DNS probes remain read-only and do not
  alter the system resolver. The user-facing override is disabled by default,
  validates unicast IPv4, persists as one `bkmap_cfg` blob, applies only after
  DHCP association, re-applies after restart/reassociation, and restores the
  captured DHCP resolvers when disabled. LAN controls, status JSON, and
  diagnostics identify DHCP versus effective DNS. No Wi-Fi credentials,
  station IP configuration, feed URLs, SNTP hostnames, TLS policy, transport
  timeouts, GPIO, PCB, or physical-output policy changed. Details are in
  `docs/PHASE4U_DNS_RESILIENCE.md`.
- **Validation:** all 15 host runners passed **1,554,232 checks, 0 failures**;
  focused LAN tests passed **279 checks**, and diagnostics tests passed
  **112 checks**. All four clean builds passed: `brooklyn-s3` 36,268 B RAM /
  304,737 B flash; `xiao-s3` 35,380 B / 290,053 B;
  `brooklyn-s3-network` 88,932 B / 1,086,577 B; `xiao-s3-network`
  88,044 B / 1,069,957 B. `git diff --check` passed. Both network ELFs have
  none of the audited physical-output initialization, arm/transmit,
  `Adafruit_NeoPixel::show()`, or safety-controller symbols; all environments
  keep `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`.
- **XIAO validation:** verified COM4 identity
  `94:A9:90:D0:94:54`; only `xiao-s3-network` was uploaded. Four direct UDP/53
  DNS checks succeeded against `1.1.1.1` and `8.8.8.8`. With manual DNS
  `1.1.1.1` enabled, NTP and MTA DNS, raw TCP/443, SNTP, and polling succeeded;
  feeds completed **8/8**. After reboot and DHCP association, the setting
  persisted and was reapplied. Disabling it restored the hotspot's DHCP DNS
  value (`0.0.0.0`); a multicast DNS address was rejected with HTTP 400 and
  did not alter the disabled state.
- **Current device state:** XIAO connected at `192.168.137.25`, Phase 4U
  firmware, manual DNS disabled, effective source DHCP, physical LED output
  disabled, brightness 64, and all 18 routes visible (`262143`). This hotspot
  still supplies DHCP DNS `0.0.0.0`; with the override disabled the latest
  status reported `NO FEEDS` (0/8), while the enabled-resolver test completed
  8/8 feeds. No production firmware was uploaded and the production PCB was
  not accessed.
  Tailscale and Windows network configuration were not changed.
- **Changed files:** `include/lan_web_policy.h`, `include/lan_web_ui.h`,
  `include/network_diagnostics.h`, `include/network_service.h`,
  `include/normal_config.h`, `src/lan_web_policy.cpp`, `src/lan_web_ui.cpp`,
  `src/network_diagnostics.cpp`, `src/network_service.cpp`,
  `src/normal_config.cpp`, `src/normal_config_esp.cpp`,
  `tools/host_tests/lan_web_policy_test.cpp`,
  `tools/host_tests/network_diagnostics_test.cpp`,
  `tools/run_lan_web_policy_tests.py`,
  `tools/run_network_diagnostics_tests.py`,
  `docs/PHASE4U_DNS_RESILIENCE.md`, and this file.
- **Historical next action:** Phase 4U was complete. Phase 4V was later
  explicitly authorized and is tracked in the current checkpoint above.

### Phase 4R hardware validation history (historical)

- **Checkpoint date:** 2026-10-17.
- **Release commit / HEAD:** `57886a3b3a5927e77b3a0e3630e92e926a60782f`,
  `Phase 4R: add LAN configuration and status UI`, on `main`.
- **Tag / remote:** annotated tag `phase-4r-lan-configuration-ui`
  (`Phase 4R LAN configuration UI validated on XIAO`) points to the release
  commit. The commit and tag were pushed to `origin`; remote `main` was
  verified at the release commit. Previous baseline was Phase 4Q
  `7d7f21e255510b09820df0f6d3a7f0f552b69b35`.
- **Phase 4R status:** final review, commit, tag, and push are complete. Live
  XIAO validation was completed on the PC Mobile Hotspot, including one
  Change-Wi-Fi handoff and USB reset back to the saved network. Final
  regression, clean builds, and ELF audit passed. Phase 4S is tracked in the
  current checkpoint above.
- **Hardware action:** uploaded `xiao-s3-network` to the authorized XIAO on
  COM4. Esptool verified the upload. Image SHA-256:
  `6A2732A5B5A63A51DB274CC63710B2F7398A8C9D4C22E1719799B147ACEB5B3A`
  (1,050,144 bytes). No production upload.
- **Latest live validation (2026-10-17):** Joel confirmed the normal
  Phase 4R page visually in a browser. PC Mobile Hotspot remained at
  `192.168.137.1/24`; XIAO DHCP address was `192.168.137.31`, then changed
  after reboot to `192.168.137.25` (the latter matched USB MAC
  `94-A9-90-D0-94-54`). IP at the end of Phase 4R validation:
  `192.168.137.25`.
  `GET /` and `/api/status` both returned HTTP 200 with correct content
  types; JSON parsed successfully. The page displayed Phase 4R status,
  connected network/IP/RSSI, display/feed state, brightness 64, and controls
  for Change Wi-Fi/Reboot. It included numeric and mDNS links, no password
  field, and no external CDN/font/script dependencies. API reported connected,
  RSSI -25, mDNS true, time synchronized, target XIAO, LED output false.
  `brooklynmap.local` resolved to the current DHCP address, and both hostname
  URLs served successfully.
- **Brightness/reboot:** initial value 64; POST to brightness 73 succeeded
  (303 redirect to `/`); API and page both showed 73. Reboot POST caused an
  actual reboot. COM4 confirmed `bkmap_cfg` brightness 73 loaded from NVS,
  Wi-Fi credentials source NVS, mDNS and HTTP startup, SNTP sync, and an
  immediate COMPLETE 8/8 poll; API returned at the new DHCP address with
  brightness 73. Brightness was restored to 64 (POST 303); page/API reported
  64, and a second reboot again loaded 64 from NVS and returned to the same
  LAN/API with SNTP and a COMPLETE 8/8 cycle.
- **HTTP/mDNS safety:** normal unknown path returned 404, and GET `/reboot`
  returned 404 without reboot (uptime continued). PC/TLS fetch of the public
  MTA feed returned HTTP 200. The full secret scan found zero occurrences of
  local SSID/password values outside ignored `include/secrets.h`; values were
  never printed. Hardware HTML/API/reboot/brightness responses contained no
  password.
- **HTTP/MTA coexistence and DNS condition:** 279 successful `GET /` plus
  279 successful `GET /api/status` over 105 seconds, then another concurrent
  70 of each over 110 seconds; zero HTTP/state errors. Four poll cycles
  occurred during the latter run, each `NO FEEDS` 0/8. Serial recorded
  `hostByName(): DNS Failed for api-endpoint.mta.info` for all eight feeds,
  with 8-9 ms failures, 0 downloaded bytes, Wi-Fi unavailable 0, and time
  unavailable 0. PC DNS resolved the MTA host and PC HTTPS GET returned 200;
  this points to DNS/upstream behavior on the PC hotspot path, not a Phase 4R
  credential failure. No transport policy was changed. LAN UI stayed
  responsive; SNTP remained true, credentials stayed in NVS, and no
  provisioning was triggered.
- **Memory/safety:** during HTTP/polling, internal free heap varied from
  about 245,948 to 247,700 B; internal minimum was around 234,292 B after
  repeated queries/polls. Free PSRAM stayed around 7,844,479-7,846,807 B;
  PSRAM minimum was 7,835,167 B. No sustained decline, reset, watchdog, or
  Guru Meditation observed. Serial heartbeats and API consistently reported
  LED outputs disabled; `physicalLedOutputEnabled=false`. No production
  hardware was touched.
- **Change-Wi-Fi handoff:** one POST handoff was exercised. Serial showed the
  normal LAN HTTP server stopped before `BrooklynMap-Avi` started at
  `192.168.4.1`; no replacement credentials were entered. An initial
  RTS-only pulse did not reset the board. A reset-only esptool USB-JTAG
  `chip_id` transaction (no flash write) then reset it; the XIAO automatically
  returned to the saved PC hotspot, and API confirmed connected, brightness
  64, mDNS, and LED output disabled. PC remained on its hotspot throughout.
- **Initial serial observations (historical, before hotspot re-provisioning):**
  XIAO startup loaded Wi-Fi credentials from NVS;
  `bkmap_cfg` had no saved brightness, so logical default 64 was used.
  512 KiB feed buffer allocation in PSRAM succeeded. Heartbeats showed LED
  outputs disabled. Station association retried and observed status 1
  (`WL_NO_SSID_AVAIL`); no association, DHCP, SNTP, normal LAN HTTP/mDNS, or
  MTA cycle occurred. Credentials were retained and provisioning did not
  start. No password was printed. No physical LEDs were connected.
- **Partial ship Wi-Fi retry (historical):** PC stayed on the existing Wi-Fi
  network
  (IPv4 `10.111.14.47/23`, gateway `10.111.15.254`); no Mobile Hotspot was
  created. With the user's approval, sent `W` over COM4. Serial confirmed
  `BrooklynMap-Avi` at `192.168.4.1` and explicitly reported stored
  credentials kept. The user submitted the current network credentials
  directly in the portal; no password was requested or captured in chat.
  Subsequent COM4 capture showed a normal `COMPLETE` MTA cycle with feeds
  8/8, 64 trains, 60 LEDs, 4 collisions, and zero feed decode failures.
  A cycle-3 memory report showed PSRAM free 7,846,323 B (minimum
  7,775,143 B), internal free heap 247,740 B (minimum 195,364 B, largest
  block 188,404 B). Heartbeats continued with LED outputs DISABLED.
  That capture did not include association-success, DHCP-IP, mDNS-startup, or
  SNTP-success lines, so these are not yet verified for the retry.
- **Follow-up ship-Wi-Fi diagnostics (historical):** a later COM4 capture
  observed cycle 79 as `COMPLETE`, feeds 8/8, 69 trains, 61 occupied LEDs,
  7 collisions, and zero decode failures. Counters reported Wi-Fi unavailable
  0, time unavailable 0, secure-connect failures 1, HTTP failures 0, and
  timeouts 5. This functionally confirmed station connectivity and valid time
  during successful polling, although explicit association/DHCP and SNTP
  startup lines were not captured. Cycle-79 memory was PSRAM free 7,846,323 B
  (minimum 7,775,143 B), internal free heap 247,760 B (minimum 195,364 B,
  largest block 188,404 B); heartbeat still reported LED outputs DISABLED.
- **Earlier ship-Wi-Fi reachability retry (superseded by hotspot run above):**
  PC Wi-Fi remained
  `10.111.14.47/23` via gateway `10.111.15.254`. The XIAO USB serial hardware
  identifier is `94-A9-90-D0-94-54`; Windows learned that same MAC at
  `10.111.15.91`, making this the likely current XIAO DHCP address (the
  serial DHCP line itself was not captured). Windows selected its direct
  Wi-Fi route for this address. ICMP ping timed out (3/3); TCP port 80
  timed out; `brooklynmap.local` did not resolve and a direct mDNS query had
  no response. Tailscale was reported disabled by the user; Windows still
  showed the virtual adapter as Up, but the target subnet route was via Wi-Fi.
  Direct PC-to-XIAO HTTP reachability is not established.
- **Earlier reachability probes (superseded):** direct multicast DNS query for
  `brooklynmap.local` produced no answer, and a port-80 scan limited to the
  PC's connected `10.111.14.0/23` subnet found no Brooklyn UI signature.
  Because there is no HTTP response even though the target IP resolves to the
  XIAO's USB hardware MAC, wireless/client isolation or network access policy
  is possible. Isolation has not been confirmed from the access-point
  configuration; this is not evidence of a firmware defect.
- **Host-only hardening:** framework-source audit confirmed
  `WebServer::on()` appends handlers on every call. Fixed a real repeated
  provisioning-start leak by guarding Phase 4L portal route registration
  across stop/start; added host coverage. Removed per-main-loop SSID/IP
  `String` allocation by capturing network identity once per LAN server
  start. Display/LED counts in status now remain sourced from the
  Phase 4M operational-display snapshot instead of potentially diverging
  pipeline candidate counts. HTML C0/DEL control bytes are emitted as numeric
  entities, with expanded HTML/JSON escaping coverage.
- **Privacy cleanup:** replaced the local SSID in the new Phase 4R policy test
  and in the existing Phase 4L provisioning test/document with generic
  `TestNetwork`. The ignored `include/secrets.h` remains untracked; a full
  tracked/untracked scan found no stored Wi-Fi password or SSID value outside
  that ignored secrets file.
- **Host validation after final hardening:** all thirteen runners passed:
  Phase 4R 98; Wi-Fi provisioning 313; train pipeline 4,538; TrainFrame
  5,232; RGB renderer 71,785; resolver 958,614; previous stop 8,098;
  physical-output safety 3,630; operational display 1,534; GTFS-RT 429;
  GTFS adapter 7,383; feed transport 1,209; output bridge 476,271. Total:
  1,539,134 checks, 0 failures. Representative host page/status responses
  were 3,287 B HTML / 660 B JSON.
- **Clean builds after final source changes:** `brooklyn-s3` 36,236 B RAM /
  304,429 B flash; `xiao-s3` 35,348 B / 289,741 B;
  `brooklyn-s3-network` 82,596 B / 1,064,457 B; `xiao-s3-network`
  81,708 B / 1,049,889 B. Target DWARF sizes: `DeviceConfiguration` 16 B,
  `LanWebUi` 508 B.
- **Output audit:** all environments keep
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`; both network builds explicitly set
  `BROOKLYN_NETWORK_ENABLED=1`, and the non-network targets use the header
  default of zero. Production network ELF has no project strip construction,
  physical transmit, `Adafruit_NeoPixel::show`, or physical-output
  safety-controller symbols. Diagnostic query helpers and Arduino's generic
  `neopixelWrite` helper remain linked; no project call path to that helper
  was found.
- **Credential audit:** local ignored `include/secrets.h` is present. Its
  SSID/password values had zero matches in tracked and untracked project files
  outside that header; the scan did not print credential values.
- **Future idea (low priority, outside Phase 4R):** Joel noted laptop
  Tailscale may interfere with local-device validation and suggested
  investigating whether the map could join his Tailscale network. Do not add
  Tailscale support or dependency in this phase; assess ESP32-S3 feasibility,
  security, resource usage, and operational requirements separately if
  requested.
- **Final verification:** all 13 host runners passed again after hardware
  validation: 1,539,134 checks, 0 failures. Four clean builds passed at the
  sizes above. Effective flags retain `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`;
  the production network ELF contains no project physical renderer, strip
  construction/transmit, `Adafruit_NeoPixel::show`, or Phase 4Q output
  controller symbols. No firmware was uploaded after the authorized XIAO
  image. Keep the manufactured PCB untouched.
- **Phase 4R closeout:** the final review, release commit/tag, remote push,
  resume checkpoint update, and clean-worktree check were completed. The
  low-priority Tailscale idea remains separate and unimplemented.

## Recovery when the manufactured PCB is available

1. Read this file and `docs/PRODUCTION_PCB_BRINGUP_CHECKLIST.md`.
2. Confirm no uncommitted firmware work would be overwritten; do not assume
   the old Phase 4Q tag is the current HEAD.
3. Keep external 5-V/barrel supply disconnected; connect USB only.
4. Keep physical LED output disabled.
5. Perform the USB-only checklist in order.
6. Stop and report results before any external LED-power step.
