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

## Current checkpoint — Phase 4R hardware validation complete, checkpoint pending

- **Checkpoint date:** 2026-10-17.
- **Baseline / HEAD:** Phase 4Q commit
  `7d7f21e255510b09820df0f6d3a7f0f552b69b35`, tag
  `phase-4q-safe-output-arming`, on `main`.
- **Phase 4R status:** implementation remains uncommitted. Live XIAO
  validation was completed on the PC Mobile Hotspot, including one
  Change-Wi-Fi handoff and USB reset back to the saved network. Final
  regression, clean builds, and ELF audit passed. Only full diff review,
  commit/tag/push, and clean-worktree verification remain. Do not start
  Phase 4S.
- **Hardware action:** uploaded `xiao-s3-network` to the authorized XIAO on
  COM4. Esptool verified the upload. Image SHA-256:
  `6A2732A5B5A63A51DB274CC63710B2F7398A8C9D4C22E1719799B147ACEB5B3A`
  (1,050,144 bytes). No production upload.
- **Latest live validation (2026-10-17):** Joel confirmed the normal
  Phase 4R page visually in a browser. PC Mobile Hotspot remained at
  `192.168.137.1/24`; XIAO DHCP address was `192.168.137.31`, then changed
  after reboot to `192.168.137.25` (the latter matched USB MAC
  `94-A9-90-D0-94-54`). Current IP at checkpoint: `192.168.137.25`.
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
- **Next action:** review/stage only intentional Phase 4R files, run
  `git diff --check`, commit using the authorized Phase 4R message, create
  the annotated Phase 4R tag, push `main` and that tag, then verify a clean
  worktree. Do not start Phase 4S.

## Recovery when the manufactured PCB is available

1. Read this file and `docs/PRODUCTION_PCB_BRINGUP_CHECKLIST.md`.
2. Confirm no uncommitted firmware work would be overwritten; do not assume
   the old Phase 4Q tag is the current HEAD.
3. Keep external 5-V/barrel supply disconnected; connect USB only.
4. Keep physical LED output disabled.
5. Perform the USB-only checklist in order.
6. Stop and report results before any external LED-power step.
