# Brooklyn Active Subway Map — Product Roadmap

This document describes a planned path from the current Phase 2A software LED
engine to a maintainable finished product. It is planning documentation only:
it does not authorize implementation of any future phase. Each hardware-affecting
step requires review against the manufactured board and its safety constraints.

## 1. Current Baseline

- **Manufactured hardware authority:** `Brooklyn_Active_Subway_Map_V1.1.eprj2`
- **Production MCU:** ESP32-S3-WROOM-1U-N16R8
- **Memory:** 16 MB flash / 8 MB OPI PSRAM
- **LEDs:** 306 addressable RGB LEDs across eight independent channels
- **Phase 1:** Safe hardware initialization is complete.
- **Phase 2A:** Software-only LED engine, topology model, XIAO simulator, and
  self-test are complete.
- **Git tags:**
  - `phase-1-safe-hardware`
  - `phase-2a-led-engine`

The XIAO ESP32-S3 remains a bench/toolchain target, not an electrical proxy for
the manufactured PCB. `PROJECT_CONTEXT.md`, `AGENTS.md`, and the manufactured
board audit remain governing references for hardware facts and safe firmware
work.

## 2. LED System

Plan and validate the LED system in stages:

1. Add a physical WS2812-compatible output backend only after separate review.
   Keep the Phase 2A framebuffer and XIAO simulation independent of the driver.
2. Support eight output channels with the manufactured order and counts:
   37, 38, 37, 37, 42, 37, 38, and 40 LEDs.
3. Define explicit safe startup and shutdown sequencing. Initialize channel
   data outputs to a known low state before considering any output enable.
   Keep production `LED_ENABLE` (GPIO1) LOW during reset, initialization,
   errors, recovery, and any state where valid output is not assured. Enabling
   level shifters requires a separately reviewed bring-up step.
4. Add a logical brightness limit and a power/current budget. Enforce global
   and, if needed, per-channel limits before generating output. Select limits
   from measured hardware behavior, not assumptions.
5. Specify startup/off behavior, loss-of-data behavior, and fault behavior:
   outputs should remain off or transition to a safe defined state on startup,
   shutdown, driver error, reset, or invalid frame.
6. Validate the assembled production PCB one channel at a time at low
   brightness, then test all channels and the full chain map.
7. Define status LED semantics for SYSTEM, NETWORK, MTA FEED, and MODE
   independently from subway route rendering.

No physical LED transmission, output pin initialization, or enable sequencing
is authorized by this roadmap.

## 3. Logical Subway Model

- Keep logical stations, route-specific station instances, track segments,
  train positions, and route identities separate from physical channel/index,
  global physical LED index, and PCB designator.
- Maintain a dedicated mapping layer from logical elements to the verified
  physical topology; do not place PCB designators in MTA parsing or train logic.
- Represent route-specific LEDs at shared stations so trains on intersecting
  routes remain distinguishable.
- Model segment LEDs explicitly, including direction and which route owns or
  shares each segment.
- Map a train’s reported position to station and segment display states.
- Define interpolation or another deterministic policy for trains between
  stations; account for missing, ambiguous, or discontinuous position updates.
- Preserve direction and route identity in the display state, including trains
  with identical station locations on different routes.
- Validate the logical-to-physical mapping against source data and reviewed
  visual/PCB references before any production rendering.

## 4. MTA Realtime Integration

- Identify the applicable GTFS-Realtime feeds and document feed URLs,
  supported entities, route identifiers, and service assumptions.
- Implement bounded feed acquisition with connection/read timeouts and
  appropriate TLS validation.
- Parse the protobuf feed with explicit size, version, and malformed-input
  handling. Keep acquisition, parsing, route filtering, and display mapping as
  separate responsibilities.
- Filter to configured routes and retain stable train identity where provided.
- Handle stale timestamps, duplicate or missing updates, canceled trips,
  detours, service changes, and route/stop identifiers that do not match static
  configuration.
- Define behavior for stale data, unsupported feed content, no active trains,
  partial feeds, and total feed failure. Do not present old positions as live.
- Use bounded retry/backoff with jitter where appropriate; avoid starving UI,
  controls, or watchdog servicing during network failures.
- Provide diagnostics for last successful fetch, feed age, entity counts,
  parse/acquisition errors, and retry state without exposing credentials.

## 5. Wi-Fi and Configuration

- Support normal Wi-Fi station-mode connection with visible connection and
  failure status, bounded retries, and recovery from an unavailable AP.
- Preserve the manufactured setup/recovery contract:
  - Setup SSID: `BrooklynMap-Avi`
  - Configuration page: `192.168.4.1`
  - Hold rotary push for 10 seconds to enter Wi-Fi recovery/setup mode.
- Implement a clear first-boot flow when credentials are absent.
- Store credentials safely in persistent settings; avoid printing secrets.
- Define factory reset behavior and distinguish it from Wi-Fi-only recovery.
- Decide whether configuration backup/restore is useful; if supported, validate
  imported data and exclude secrets unless a protected export mechanism exists.
- Keep setup AP lifetime, access policy, and transitions documented and
  testable.

## 6. User Interface

- Implement rotary interaction according to the manufactured labels:
  - **TURN — NAVIGATE**
  - **PRESS — SELECT**
- Specify debouncing, long-press timing, repeat behavior, and simultaneous
  button handling.
- Define the behaviors for TEST, ROUTE, DIM, POWER, MODE, and BRIGHT before
  wiring each action into the UI.
- Define OLED screens and transitions only after the installed display module,
  controller, and resolution are confirmed.
- Assign clear status behavior to the SYSTEM, NETWORK, MTA FEED, and MODE
  status LEDs, including startup and error states.
- Specify buzzer patterns and volume/duration constraints; keep sound disabled
  until explicitly implemented and tested.
- Ensure feed, Wi-Fi, settings, and hardware errors have understandable
  indications and do not cause unsafe output.

## 7. Persistent Settings

- Persist brightness limit, operating mode, selected routes/features, and
  Wi-Fi configuration with explicit defaults and validation.
- Use a versioned configuration format with bounds checks, integrity checking,
  and safe recovery from missing, corrupt, or partially written data.
- Define migration steps between configuration versions before changing the
  schema. Preserve user settings where safe; otherwise report reset/migration
  outcomes clearly.
- Test power loss during writes, full storage, invalid values, factory reset,
  and upgrade/downgrade compatibility expectations.

## 8. OTA Updates

- Use semantic firmware versioning and identify supported hardware versions in
  each build and release.
- Confirm dual OTA application partitions fit the production flash layout and
  retain a manual USB recovery path.
- Require boot validation before marking an updated image healthy; roll back
  automatically after failed boot validation or a critical startup failure.
- Retrieve updates over HTTPS with certificate validation. GitHub Releases may
  be the publication source, subject to a documented release/asset convention.
- Publish a version manifest containing supported hardware, firmware version,
  artifact location, and integrity hash.
- Verify artifact integrity before installation. Consider signed update
  metadata and key rotation/revocation planning before broad deployment.
- Specify update interruption, power loss, network loss, invalid image, and
  rollback behavior. Never erase the known-good image before the candidate is
  validated.
- Keep manual USB recovery instructions available for failed-update recovery.

## 9. Watchdog and Reliability

- Define watchdog ownership, service intervals, and which tasks may block.
- Handle brownout/reset causes explicitly; start in the safe hardware state
  after every reset.
- Record actionable crash/reset diagnostics while bounding persistent writes.
- Recover Wi-Fi and feed acquisition independently without reboot loops.
- Define degraded operation during MTA outages, Wi-Fi outages, malformed feeds,
  and persistent-storage errors.
- Ensure faults leave LED output disabled or in a separately reviewed safe
  state; never leave uncontrolled high-brightness output after a fault.
- Provide serial and, where appropriate, on-device logging with timestamps,
  severity, bounded storage, and privacy-safe content.
- Test repeated failures and recovery paths, not only nominal startup.

## 10. Hardware Archival

Maintain an immutable archive for the exact V1.1 manufacturing run, including:

- Original EasyEDA V1.1 source project.
- Gerbers actually sent to and used by the manufacturer.
- BOM actually manufactured, including approved substitutions.
- CPL / pick-and-place files actually used.
- Schematic PDF and PCB drawings/renders.
- Assembly configuration, panelization, stencil, and process notes where
  available.
- Component substitutions and deviations from the nominal BOM.
- Manufacturing notes, order identifiers, vendor, version, and order/date
  information.
- SHA-256 hashes for every archived artifact and a manifest that identifies
  the actual manufacturing set.

**Manufactured V1.1 artifacts must never be overwritten in place.** Original
manufacturing artifacts are archival records. Later PCB revisions must use new
hardware version numbers and new immutable artifact directories; do not replace
or relabel V1.1 files as a later design.

## 11. Documentation

Build and maintain documentation as the corresponding features are reviewed
and implemented:

- `README.md` — project overview, status, targets, and quick start.
- `USER_GUIDE.md` — controls, operating modes, route/status behavior, and
  routine use.
- `HARDWARE_BRINGUP.md` — safe staged production-board validation and measured
  electrical checks.
- `DEVELOPMENT.md` — toolchain, build, test, branch, and contribution workflow.
- `OTA_UPDATES.md` — update process, validation, rollback, and USB recovery.
- `WIFI_SETUP.md` — first boot, normal connection, setup AP, recovery, and
  credential handling.
- `TROUBLESHOOTING.md` — symptoms, diagnostics, safe recovery, and escalation.
- `MANUFACTURING.md` — immutable artifact archive, run records, and revision
  policy.
- `CHANGELOG.md` — user-visible changes by release.
- `RELEASE_PROCESS.md` — repeatable build, validation, publication, and rollback.
- `LICENSE` and third-party attribution — project licensing and notices for
  dependencies, assets, and incorporated material.
- Photographs and PCB images — labeled production-board views and assembly
  references, with revision/run provenance.

Keep pin, manufacturing, and electrical claims traceable to the authoritative
board artifacts or documented measurements.

## 12. Repository Structure

A future organization may resemble:

```text
firmware/
  include/
  src/
  test/
hardware/
  v1.1/
    source/
    gerbers/
    bom/
    cpl/
    drawings/
    manifests/
docs/
tools/
releases/
```

Reorganization should happen only at a controlled milestone, with paths,
builds, CI, documentation links, and archival references migrated together.
Do not reorganize the repository during active firmware development or combine
that effort with hardware-affecting changes.

## 13. Release Process

For every release:

1. Select and document a semantic version and supported hardware revision(s).
2. Review scope, configuration migrations, known issues, and release notes.
3. Build both `xiao-s3` and `brooklyn-s3`; retain logs and toolchain versions.
4. Run automated tests and the applicable bench/production validation plan.
5. Generate firmware binaries and a manifest; calculate and independently
   verify SHA-256 hashes.
6. Review binaries, partition layout, version identity, and recovery procedure.
7. Create a Git tag and publish a GitHub Release with notes, artifacts, hashes,
   and supported-hardware information.
8. Publish the approved OTA manifest/artifacts only after release checks pass.
9. Monitor update health and document a rollback or withdrawal procedure.

Do not publish a production OTA release until signing/integrity, rollback,
recovery, and supported-board checks have passed.

## 14. Testing

- **XIAO bench testing:** compile-time target isolation, serial diagnostics,
  self-tests, framebuffer operations, simulator output, and command handling.
- **Production PCB bring-up:** verify power rails and safe reset state before
  enabling any LED buffers; preserve USB CDC and check each peripheral.
- **LED channels:** validate exact topology/designator order, one channel at a
  time at low brightness, then channel interactions and full-map behavior.
- **Controls:** debounce, press/release, long hold, simultaneous input, and
  recovery behavior.
- **Wi-Fi:** first boot, normal connection, disconnect/reconnect, setup AP,
  invalid credentials, and recovery timeout behavior.
- **MTA feeds:** recorded valid feeds, malformed/oversized feeds, stale data,
  service changes, filtering, timeouts, and outage/retry behavior.
- **OTA:** valid/invalid hashes, interrupted transfer, power loss, failed boot,
  rollback, manual USB recovery, and supported-hardware rejection.
- **Power/current:** measured LED current across representative patterns,
  brightness limits, startup, and fault/off states.
- **Soak tests:** long-duration operation with realistic feed and UI activity,
  heap/resource monitoring, reconnection, and watchdog checks.
- **Failure injection/recovery:** brownout/reset, sensor/feed/network faults,
  corrupt settings, blocked services, failed OTA, and confirmation of safe LED
  state after each fault.

## 15. Phased Development Plan

Each phase is separately scoped, reviewed, tested, and authorized before
implementation. This roadmap itself authorizes none of these changes.

| Phase | Planned scope | Exit criteria |
|---|---|---|
| **2B — Physical LED backend design** | Select and isolate a driver/backend; define channel interface, output sequencing, brightness and current budgets. | Reviewed design and tests demonstrate no production output enable or data transmission before explicit bring-up approval. |
| **2C — Production LED bring-up** | Validate electrical rails, buffers, data integrity, and channel mapping on the manufactured PCB. | Recorded low-brightness channel-by-channel test and verified safe shutdown/fault behavior. |
| **3 — Logical subway model** | Stations, route identity, shared stations, segments, train positions, and physical mapping. | Model and mapping tests cover shared routes, between-station positions, direction, and missing data. |
| **4 — Wi-Fi/configuration** | Normal Wi-Fi, setup/recovery AP, web configuration, credentials, first boot, factory reset. | Recovery contract and configuration failure cases pass on bench and production hardware. |
| **5 — MTA feed integration** | GTFS-Realtime acquisition, parser, filtering, train identity, stale/failure handling, diagnostics. | Recorded-feed and failure tests pass with bounded retries and clear stale/outage behavior. |
| **6 — Controls/UI** | Rotary, front buttons, OLED, status LEDs, buzzer, and user-facing error/status behavior. | Control contract, OLED behavior, and status/error indications are documented and validated. |
| **7 — OTA/reliability** | Versioning, OTA partitions, integrity/signing plan, rollback, watchdog, crash/network recovery. | Interrupted/invalid update, rollback, recovery, and fault-state tests pass. |
| **8 — Documentation/hardware archive** | Complete user/developer/manufacturing docs and immutable V1.1 archive with verified hashes. | Independent archive/hash audit and documentation review complete. |
| **9 — Release candidate and soak testing** | Freeze candidate, run both builds, full regression, hardware/power checks, and long-duration soak. | No release-blocking defects; test results and known limitations approved. |
| **10 — Production release** | Publish versioned binaries, manifest, Git tag, GitHub Release, OTA assets, and recovery instructions. | Release checks, provenance, rollback plan, and post-release monitoring are ready. |

Phase ordering may be adjusted through an explicit review when dependencies or
test evidence require it. Any production hardware change needs a new hardware
revision and immutable manufacturing records.
