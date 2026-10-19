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

## Current checkpoint — Phase 4AA telemetry design

- **Checkpoint date:** 2026-10-19. Started from clean `main` at
  `13c1cf86ccaf462c56f6afc8e39dbc1c15817c7c`
  (`docs: update remote telemetry roadmap`), pushed to `origin`.
- **Status:** Phase 4AA MQTT architecture/protocol/privacy design is FROZEN and
  committed as the engineering checkpoint tag `phase-4aa-telemetry-design`.
  Firmware telemetry, automatic updates, and hardware validation have **not**
  been implemented or started. Host-side collector/dashboard work follows (see
  the Phase 4AA host-side section when present).
- **Design:** [docs/PHASE4AA_REMOTE_TELEMETRY_DESIGN.md](docs/PHASE4AA_REMOTE_TELEMETRY_DESIGN.md)
  now supersedes the earlier HTTPS-POST proposal with MQTT over verified TLS
  to `mqtt.goldwein.net:8884`, unique per-device credentials, strict
  publish-only ACLs, retained status/availability, Last Will, bounded events,
  collector/SQLite/dashboard architecture, resource estimates, and staged
  implementation plan. No payload P-256 signatures are recommended for v1.
  Topic/infrastructure details are in
  [docs/PHASE4AA_MQTT_TOPIC_SCHEMA.md](docs/PHASE4AA_MQTT_TOPIC_SCHEMA.md);
  illustrative broker files are in `docs/examples/`; payload schemas are in
  `docs/schemas/`.
- **Infrastructure:** `mqtt.goldwein.net:8884` verified from Windows on
  2026-10-19: DNS, TCP, TLS 1.3 with hostname/chain verification (Let's
  Encrypt, valid to 2027-01-08), MQTT 3.1.1, anonymous CONNECT rejected,
  authenticated CONNECT, QoS 1, retained messages, retained Last Will, and
  publish-only device ACL isolation (classic `acl_file`, temporary
  `brooklynmap` test account; other-namespace/unrelated publishes not
  delivered, device receives nothing; SUBACK is granted but delivery is
  blocked). The read-only `bkmap-collector` ACL is also verified (live and
  retained delivery works; nothing outside the three topic classes is delivered
  even for `#`; its publishes are not delivered). Broker classification:
  **VERIFIED READY FOR PHASE 4AA TELEMETRY**. Still pending: Mosquitto version
  and per-device production provisioning. Port 1883 was never used and must never
  be a firmware fallback. Before valid SNTP, no MQTT/TLS connection is
  attempted and telemetry remains pending. Temporary paho venv/test scripts
  live outside the repo in `AppData\Local\Temp`; no credentials were stored.
- **Open approvals:** review credential enrollment over the current local HTTP
  UI versus a separately secured issuance service; approve Mosquitto
  configuration/auth/ACL approach; choose the MQTT library and validate
  persistent TLS RAM/scheduling; approve cadence, retention, consent wording,
  and optional display-preference telemetry. No live broker/Apache/DNS/
  firewall changes were made.
- **Product status:** [ROADMAP.md](ROADMAP.md) records Phase 4AA as
  DESIGN COMPLETE / IMPLEMENTATION NOT STARTED. Monitoring remains opt-in,
  disabled by default, outbound-only, and monitoring-only. Tailscale is not
  required; automatic updates remain separate future work.
- **Validation and scope:** Proposed JSON schemas parse; MQTT design/schema
  and Mosquitto examples still require review and version-specific validation.
  No live broker diagnostic or configuration check was run; no local JSON
  Schema meta-validator is installed. Firmware tests/builds were not run
  because no firmware changed. No files under `src/`, `include/`, embedded
  libraries, firmware configuration, LAN UI, NVS implementation,
  `network_service`, or OTA code were changed. No dependencies were added.
- **Hardware/safety:** no firmware was uploaded; XIAO and production PCB remain
  untouched. No physical LED output was enabled.
- **Git policy:** the design is committed with tag `phase-4aa-telemetry-design`
  (engineering checkpoint only; no semantic firmware release tag). Later
  host-side commits must not move that tag. Phase 4Z
  `dea08f2ecfadb4fe06f66051b09f7391ecb4d459` and
  `phase-4z-unknown-stop-audit` remain unchanged.
- **Host-side implementation (added after the design checkpoint):** see
  [docs/PHASE4AA_COLLECTOR.md](docs/PHASE4AA_COLLECTOR.md). `tools/telemetry/`
  holds a stdlib schema validator, SQLite store, ingest/state logic, optional
  paho-mqtt live subscriber (`collect.py`, never publishes, env-only credentials)
  and a loopback-only read-only dashboard. Tests: `python tools/run_telemetry_tests.py`
  (107 checks, 0 failures; synthetic, no broker or credentials needed). All 20
  existing host runners were re-run and pass. The live subscriber and dashboard
  have not been run against the real broker (no credentials requested).
  Firmware, `platformio.ini`, broker, Apache/DNS/firewall, XIAO and production
  PCB were not touched. Secrets: none written to Git or disk.
- **Cross-boot fix (`4316896`):** a different `boot_id` is accepted only with a
  strictly later `device_time_utc`; see docs/PHASE4AA_COLLECTOR.md. Telemetry
  tests now 125 checks, 0 failures; all 20 existing runners pass.
- **Live validation:** operator-confirmed `live_e2e.py` PASS ("PASS / ALL GOOD")
  against the real broker (`mqtt.goldwein.net:8884`, verified TLS, MQTT 3.1.1,
  `bkmap-collector` + synthetic `brooklynmap` device). Per-check output was not
  preserved, so no individual timings are claimed; see "Live validation record"
  in docs/PHASE4AA_COLLECTOR.md. Credentials were env-only and not persisted.
  Decision memos are in docs/PHASE4AA_DECISION_MEMOS.md.
- **Commits (all pushed to `main`):** `c657dc8` design (tag
  `phase-4aa-telemetry-design`), `1730533` host collector, `4316896` cross-boot
  fix, `5c2bc36` live E2E harness + memos; later work is listed in the
  "Unattended hardening" entry below if present.
- **Next action:** review the MQTT protocol and schemas; separately verify
  broker infrastructure under authorization; resolve enrollment, library,
  resource, and retention decisions before authorizing any implementation.

## Current checkpoint — Phase 4Z UnknownStop evidence audit

- **Checkpoint date:** 2026-10-19. Work started from clean `main` at
  `8548257e2ec7ef65d0ed641d160399679ad982ad`
  (`docs: record Phase 4Y checkpoint push`).
- **Implementation commit/tag:** `dea08f2ecfadb4fe06f66051b09f7391ecb4d459`
  (`Phase 4Z: audit GTFS unknown-stop coverage`), tagged
  `phase-4z-unknown-stop-audit` (`Phase 4Z GTFS UnknownStop coverage audit`).
  The commit and annotated tag were pushed to `origin`. The tag points to the
  Phase 4Z implementation; later roadmap/checkpoint documentation is a
  documentation-only follow-up and must not move the tag.
- **Audit results:** all 152 route/platform pairs (429 observations) have
  static GTFS stop records. 149 pairs / 423 observations are static-served
  route positions outside route-specific map scope; two D/A21S and D/A22S
  pairs / four observations disagree with static route service (static lists
  A/B/C, not D); one route-2 Clark St pair / two observations has a mapped
  current station but previous stop 230 outside the project map. Forty
  observations across 35 pairs also reported topology `UnknownDestination`,
  consistent with the intentionally on-map-only destination table. The live
  D sequence is consistent with a route-path divergence, but its cause is
  unresolved. No mapping change is authorized by this evidence.
- **Preserved behavior:** route H at H19/H19N/H19S remains the separately
  documented `OperationalStop` with no LED. No alias, stop map, topology,
  station/segment semantic, color, or LED assignment was changed.
  The Phase 4Y capture's 100 H19N operational-stop observations remain outside
  the 429 UnknownStop observations.
- **Outputs:** [docs/PHASE4Z_UNKNOWN_STOP_AUDIT.md](docs/PHASE4Z_UNKNOWN_STOP_AUDIT.md)
  and [docs/PHASE4Z_UNKNOWN_STOP_AUDIT.csv](docs/PHASE4Z_UNKNOWN_STOP_AUDIT.csv)
  contain the evidence, route and parent/direction aggregates, nearest
  route-specific map candidates, and per-pair classifications. The deterministic
  host audit is [tools/phase4z/unknown_stop_audit.py](tools/phase4z/unknown_stop_audit.py);
  focused tests are run by `python tools\run_phase4z_tests.py`.
- **Inputs:** official MTA static GTFS ZIP and the Phase 4Y raw capture/replay
  remain local under Git-ignored `captures/`. The derived audit outputs contain
  no raw payloads, trip IDs, or credentials. All three normalized static GTFS
  member hashes match `data/gtfs_topology.csv`.
- **Validation:** Phase 4Z focused suite passed **7 tests, 0 failures**; all
  **20 host runners passed 1,554,763 checks, 0 failures**. Two full offline
  replay runs were byte-identical
  (SHA-256 `b39513529049e353c67d8b6fc9d758231afbc6c0de11a34d2c1f09cc7e75c649`).
  The audit CSV and report regenerated twice byte-identically and match the
  generated local outputs. All four clean PlatformIO builds match the Phase
  4Y baseline exactly: `brooklyn-s3` 36,268 B RAM / 305,097 B flash;
  `xiao-s3` 35,380 B / 290,413 B; `brooklyn-s3-network` 93,308 B /
  1,116,665 B; `xiao-s3-network` 92,420 B / 1,100,089 B. Both network ELF
  audits found no simulator/replay bridge or physical backend initialization,
  arming, transmission, NeoPixel `show()`, or safety-controller symbols. The
  XIAO network ELF retains its expected logical `led::render()` demo symbol;
  the Brooklyn production ELF does not. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`
  remains set in both base environments and inherited network builds.
- **Changed files:** `RESUME.md`, the Phase 4Z Markdown report and aggregate
  CSV, `tools/phase4z/unknown_stop_audit.py`,
  `tools/phase4z/test_unknown_stop_audit.py`, and
  `tools/run_phase4z_tests.py`. No firmware source, PlatformIO configuration,
  semantic mapping, topology, LED assignment, or physical-output code changed.
- **Product roadmap:** Tailscale is superseded and optional-experiment-only;
  it is not an active product requirement. [ROADMAP.md](ROADMAP.md) now names
  Phase 4AA — Opt-In Remote Operational Telemetry as the next planned product
  phase and records default-off/outbound-only/monitoring-only behavior plus
  privacy, identity, reliability, and separate signed-update direction. No
  Phase 4AA implementation has started.
- **Documentation follow-up:** [docs/TAILSCALE_REMOTE_ACCESS_PLAN.md](docs/TAILSCALE_REMOTE_ACCESS_PLAN.md)
  is marked SUPERSEDED / OPTIONAL EXPERIMENT ONLY. Its historical audit notes
  remain, but former setup/deployment guidance is explicitly not current.
  [docs/PHASE4W_SECURE_WEB_OTA.md](docs/PHASE4W_SECURE_WEB_OTA.md) now advises a
  trusted/private LAN rather than referring to the superseded remote-access
  deployment. These documentation-only changes are in `ROADMAP.md`,
  `docs/TAILSCALE_REMOTE_ACCESS_PLAN.md`,
  `docs/PHASE4W_SECURE_WEB_OTA.md`, and `RESUME.md`.
- **Safety:** no firmware source or build configuration changed. No device was
  accessed or flashed; XIAO and production PCB remain untouched.
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains unchanged. The Phase 4Y tag
  remains unchanged.
- **Final review:** `git diff --check` passed before the Phase 4Z
  implementation commit. Its worktree contained only the Phase 4Z host tooling,
  derived audit outputs, and this checkpoint. The later changes are
  documentation-only; the live capture and static GTFS remain ignored and
  untracked.
- **At the time of this Phase 4Z checkpoint:** the audit was complete and Phase
  4AA remained a future planned phase requiring separate authorization. The
  current Phase 4AA design status and approval blockers are recorded in the
  checkpoint above. The unresolved D-route discrepancy remains a separate
  evidence-review item, not an approved mapping change.

## Phase 4Y checkpoint — GTFS-RT capture and replay

- **Checkpoint date:** 2026-10-19. Phase 4Y work began on clean `main` at
  `cd4ee44b53eeb778bdbefeaa458d276ad9f83385`
  (`docs: record Phase 4X simulator checkpoint`). The Phase 4X implementation
  commit/tag remain unchanged.
- **Implementation commit/tag:** `b6a61dedc925044b6e62b72047e64b80552bd2bf`
  (`Phase 4Y: add GTFS record and replay tools`) is pushed to `origin/main`
  and tagged `phase-4y-gtfs-record-replay`. The engineering tag points to this
  implementation commit; no firmware release tag was created.
- **Implementation:** host-only tools record raw response bodies from the
  eight firmware-defined MTA GTFS-RT feeds, verify manifest/payload hashes,
  and replay those bytes through the compiled firmware decoder, pipeline,
  resolver, display policy, and renderer. The Phase 4X browser simulator
  imports replay exports and displays feed health, train details, unresolved
  observations, and renderer collisions. Capture and replay architecture,
  options, and limitations are in
  [docs/PHASE4Y_GTFS_RECORD_REPLAY.md](docs/PHASE4Y_GTFS_RECORD_REPLAY.md).
- **Live capture:** `captures/20261019T062411Z-464a7da9/` contains ten cycles
  with all eight feeds successful in every cycle. Its 80 raw `.pb` files
  total 2,267,622 bytes and were integrity-checked before replay. The local
  deterministic `replay.json` contains ten frames: 2,484 vehicles seen, 374
  resolved, all `Fresh`, and zero parser failures. It reports 429
  `UnknownStop` observations over 152 route/stop pairs and 100 `OperationalStop`
  observations for H19N. These categories are diagnostic evidence only; no
  route, station, or segment assignment was changed. The capture and replay
  remain Git-ignored third-party data and must not be committed or published.
  The capture directory was confirmed ignored and untracked at checkpoint;
  only the record/replay framework and derived counts in
  `docs/PHASE4Y_UNKNOWN_STOP_COUNTS.csv` are in Git.
  Two independent replay runs produced identical canonical output
  (SHA-256 `b39513529049e353c67d8b6fc9d758231afbc6c0de11a34d2c1f09cc7e75c649`).
  Non-payload aggregate counts for every unknown route/stop pair are retained
  in [docs/PHASE4Y_UNKNOWN_STOP_COUNTS.csv](docs/PHASE4Y_UNKNOWN_STOP_COUNTS.csv).
  Zero segment LED occupancies in this sample is not a failure; synthetic
  regressions cover segment behavior. A dedicated unknown-stop audit is the
  recommended follow-up, without presuming any mapping defect.
- **Validation:** Phase 4Y focused tests passed **74 checks**; Phase 4X
  focused tests passed **80 checks**. All **19 host runners passed
  1,554,756 checks, 0 failures**. Generated simulator metadata synchronization
  passed. Browser smoke verified 306 points, 302 audit rows, four status
  indicators, first/middle/final cycle navigation, play/pause, search,
  inspector, unresolved observations, actual firmware collision RGB, and
  autoplay. The server bound only to `127.0.0.1` and was stopped. All four
  clean PlatformIO builds and both network ELF safety audits passed earlier
  in Phase 4Y and match the Phase 4X baseline:
  `brooklyn-s3` 36,268 B RAM / 305,097 B flash;
  `xiao-s3` 35,380 B / 290,413 B;
  `brooklyn-s3-network` 93,308 B / 1,116,665 B;
  `xiao-s3-network` 92,420 B / 1,100,089 B.
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains configured.
- **Committed files:** `.gitignore`, this checkpoint,
  `docs/PHASE4Y_GTFS_RECORD_REPLAY.md`,
  `docs/PHASE4Y_UNKNOWN_STOP_COUNTS.csv`, `tools/replay/` (host bridge,
  recorder, replay CLI, validation and tests), `tools/run_replay_tests.py`,
  and `tools/virtual_map/index.html` / `run.py`. No firmware source,
  PlatformIO configuration, semantic assignments, physical-output code,
  OTA code, or authentication code changed.
- **Hardware/release state:** no firmware image was uploaded; no XIAO or
  production PCB was accessed; the production PCB remains untouched.
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains configured. No firmware
  release or subsequent phase was started.
- **Current Git state at implementation checkpoint:** branch `main` at
  `b6a61dedc925044b6e62b72047e64b80552bd2bf`, synchronized with `origin/main`.
  The `phase-4y-gtfs-record-replay` tag is pushed and remains on the
  implementation commit. The local MTA capture must remain ignored and must
  not be committed or published.
- **Resume command:** from the repository root, open the complete capture in
  the simulator initially paused with
  `python tools\replay\replay.py captures\20261019T062411Z-464a7da9 --all --serve --step`.

## Phase 4X checkpoint — host-only virtual map simulator

- **Checkpoint date:** 2026-10-19. Work started on clean `main` at
  `3ea25cc4e1256d8a74ad8b8fba7c9358289b66e7`
  (`docs: record prerelease asset upload instructions`). Phase 4W's released
  firmware and release assets were not changed.
- **Implementation:** the host-only simulator compiles the authoritative
  firmware semantic tables, `TrainFrame`, RGB renderer, and Phase 4M
  operational status overlay, then joins their output to all 306 exact V1.1
  PCB addressable-LED placements. The browser can inspect route/station/
  segment/status pixels, synthetic scenarios, status states, and JSON frames.
  It binds to loopback only and never links or calls the physical display
  bridge, LED backend, GPIO, or strip transmitter. Details, launch instructions,
  source/geometry provenance, and limitations are in
  [docs/PHASE4X_VIRTUAL_MAP_SIMULATOR.md](docs/PHASE4X_VIRTUAL_MAP_SIMULATOR.md).
- **Validation:** Phase 4X passed **80 checks**; all **18 host runners passed
  1,554,682 checks, 0 failures**. Browser smoke checks saw 306 SVG points,
  302 map-audit rows, and four status indicators. Metadata export is current.
  All four clean PlatformIO builds passed and exactly match the Phase 4W RAM/
  flash baseline. Both network ELF audits found no simulator, NeoPixel
  transmit, LED-backend arm/write, or physical safety-controller symbols.
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains set in all environments.
- **Checkpoint commit and tag:** commit
  `8e315289002f28cae3bc9b4a941de7d645157f27`
  (`Phase 4X: add virtual map simulator`) is pushed to `origin/main` and tagged
  `phase-4x-virtual-map-simulator`. The milestone tag points to the simulator
  implementation commit; no semantic firmware release tag was created.
- **Safety/state:** no firmware sources or build configuration changed; no
  device, XIAO, or production PCB was accessed or flashed. No firmware image
  was uploaded and Phase 4Y was not started. The private OTA signing key
  remains outside Git.
- **Changed files:** this checkpoint, the Phase 4X implementation and
  generated metadata under `tools/virtual_map/`, its runner
  `tools/run_virtual_map_tests.py`, and
  `docs/PHASE4X_VIRTUAL_MAP_SIMULATOR.md`. The host-only simulator milestone
  uses the annotated tag `phase-4x-virtual-map-simulator`; it is not a
  semantic firmware release.
- **Next action at that checkpoint:** start the simulator with
  `python tools\virtual_map\run.py`. This historical next step has since been
  exercised during Phase 4Y; see the current checkpoint above.

## Phase 4W status — secure web OTA validated; release wrap-up (historical)

- **Checkpoint date:** 2026-10-18. Starting baseline was clean `main` at
  `10095a40ad23aa4e1b15dbb728b6f3a2bf127d17`, tagged
  `phase-4u-dns-resilience`.
- **Implementation and complete results:**
  [docs/PHASE4W_SECURE_WEB_OTA.md](docs/PHASE4W_SECURE_WEB_OTA.md).
  Signed `.bkota` uses ECDSA P-256/SHA-256, an 84-byte authenticated header,
  raw 64-byte signature, and streamed firmware payload. Public key id 1 is
  provisioned; key id 2 is reserved. Fingerprint:
  `75cb7adec5d6d542cef403f65d9df5387816eb47a3bc237dcd27068754e93717`.
  The private key remains outside the repo at
  `C:\Users\jgold\.brooklynmap\ota\brooklyn_ota_p256.pem`; Joel must keep an
  encrypted/offline backup.
- **Validation:** all 17 host runners passed **1,554,602 checks, 0 failures**;
  all four clean PlatformIO builds and both network ELF safety audits passed.
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` in every environment.
- **XIAO only:** COM4 identity was checked (ESP32-S3, MAC
  `94:A9:90:D0:94:54`). Actual signed install, negative packages, health
  confirmation, and bootloader rollback were validated. The final normal
  Phase 4W image is restored. After the provisioning-portal check, public
  `brooklynmap.local/api/status` returned HTTP 200: Phase 4W, connected at
  `192.168.137.191`, administrator configured, brightness 64, manual DNS
  disabled, and physical output disabled.
- **Final flash check:** read-only `otadata` read showed valid sequence 3
  selecting `app0` at `0x10000`; sequence 2 in `app1` was also valid. Serial
  heartbeats continued through 60 seconds with LED outputs disabled. No
  production PCB was connected or programmed.
- **Release commit:** `e874b9f46590cdb47f86648e73d2652657ee0ac4`
  (`Phase 4W: add secure signed web OTA`), tagged
  `phase-4w-secure-web-ota` (`Phase 4W secure signed web OTA validated on
  XIAO`). Both `main` and the tag were pushed and verified on `origin`.
- **Semantic prerelease tag:** annotated `v0.1.0-rc1` points directly to
  `e874b9f`; it is pushed to `origin`. The engineering milestone tag remains
  unchanged.
- **Prerelease artifacts:** `dist/0.1.0-rc1/` contains signed packages for
  XIAO and production targets, `SHA256SUMS.txt`, and the prepared GitHub
  description in `RELEASE_NOTES.md`. Both signatures, targets, payloads against
  the tagged network-build binaries, and package checksums were verified. The
  directory contains only these four files; the private signing key remains
  outside Git. Upload exactly `xiao-s3-network-0.1.0-rc1.bkota`,
  `brooklyn-s3-network-0.1.0-rc1.bkota`, and `SHA256SUMS.txt`; paste
  `RELEASE_NOTES.md` as the release description, not as an asset.
- **GitHub Release:** not created. `gh` is not installed and the available
  GitHub integration has no release-creation operation; do not install tooling
  or use credential workarounds. Joel must manually create and publish the
  prerelease, selecting tag `v0.1.0-rc1`, title `Brooklyn Map Firmware
  0.1.0-rc1`, checking the prerelease option, and uploading only the two
  `.bkota` packages and `SHA256SUMS.txt`. Paste `RELEASE_NOTES.md` as the
  description. The notes explicitly state production-PCB OTA is unvalidated.

## Phase 4V history — LAN access protection validated on XIAO

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
- **Captive-portal audit (post-4V):** wildcard captive DNS already existed;
  no firmware change was needed or kept. Android auto-popup was not achieved
  on the user's phone (it probes `/generate_204`, then leaves); manual
  `http://192.168.4.1/` is the accepted setup path; arbitrary plain-HTTP
  hostnames also reach the setup page. Temporary 302/logging experiments were
  reverted. Details in
  `docs/PHASE4L_WIFI_PROVISIONING.md`. The XIAO runs the released Phase 4V
  image.
- **Historical remote-access audit (superseded):**
  `docs/TAILSCALE_REMOTE_ACCESS_PLAN.md` recorded an earlier optional Tailscale
  subnet-router experiment. Tailscale remote access is no longer an active
  product requirement; no network changes were made during that audit.
  Tailscale is optional experiment only.
- **Historical next action:** Phase 4V was released (tag
  `phase-4v-lan-access-protection`); Phase 4W was subsequently authorized and
  released. The current work is the Phase 4X host-only simulator above.

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
- **Superseded product idea (historical, optional experiment only):** an earlier
  note considered connecting the map to a Tailscale network. That is no longer
  an active product requirement or planned monitoring architecture. Do not
  infer a Tailscale implementation task from this historical note.
- **Final verification:** all 13 host runners passed again after hardware
  validation: 1,539,134 checks, 0 failures. Four clean builds passed at the
  sizes above. Effective flags retain `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`;
  the production network ELF contains no project physical renderer, strip
  construction/transmit, `Adafruit_NeoPixel::show`, or Phase 4Q output
  controller symbols. No firmware was uploaded after the authorized XIAO
  image. Keep the manufactured PCB untouched.
- **Phase 4R closeout:** the final review, release commit/tag, remote push,
  resume checkpoint update, and clean-worktree check were completed. The
  historical Tailscale idea remains unimplemented and is superseded as a
  product requirement; it is optional experiment only.

## Recovery when the manufactured PCB is available

1. Read this file and `docs/PRODUCTION_PCB_BRINGUP_CHECKLIST.md`.
2. Confirm no uncommitted firmware work would be overwritten; do not assume
   the old Phase 4Q tag is the current HEAD.
3. Keep external 5-V/barrel supply disconnected; connect USB only.
4. Keep physical LED output disabled.
5. Perform the USB-only checklist in order.
6. Stop and report results before any external LED-power step.
