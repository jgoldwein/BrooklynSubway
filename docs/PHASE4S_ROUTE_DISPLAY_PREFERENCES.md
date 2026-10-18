# Phase 4S: persistent route-display preferences

## Scope and canonical route set

Phase 4S adds persistent user control over which subway routes are rendered on
the logical map. Feed downloads, GTFS-RT decoding, train resolution,
`TrainPipeline`, `TrainFrame` occupancy, and Phase 4M freshness decisions
continue to process and retain all routes. Visibility is applied only by the
existing `semantic::renderTrainFrame()` RGB renderer.

The canonical source is the Phase 3B generated `semantic::Route` enum and
`kRouteNames` table in `include/generated/semantic_tables.h` and
`src/generated/semantic_tables.cpp`. The source defines 18 routes, in order:

`2, 3, 4, 5, A, B, C, D, F, G, J, L, M, N, Q, R, S, Z`

The existing enum values 1 through 18 define stable mask positions: route enum
value `n` maps to bit `n - 1`. `Route::None` and invalid enum values have no
visibility bit. This reuses canonical route identity and does not add another
route-identity or color table.

## Configuration and migration

Preferences use the existing NVS namespace `bkmap_cfg`, independently of
Wi-Fi credentials in `bkmap_wifi`:

- `brightness`: existing `uint32_t` setting, default 64, range 0–255.
- `route_mask`: new `uint32_t` visibility mask; bits 0–17 correspond to the
  canonical route order above.

The default mask is `0x0003FFFF` (all 18 routes visible). Mask zero is a valid
intentional setting (no routes visible). Bits above bit 17 are normalized away
when loading or saving. Missing or unreadable route data defaults to all
visible. No namespace is cleared and no Wi-Fi key is read or written by the
route preference.

Loading each setting independently preserves upgrade compatibility. A Phase
4R device with a saved brightness and no `route_mask` retains its brightness
and gets the all-visible default. Brightness updates do not reset route
visibility; route updates do not reset brightness. Each explicit form save
performs one NVS write, and an unsuccessful write leaves the in-memory value
unchanged.

## Rendering ownership and behavior

The path is:

```text
all GTFS feeds -> TrainPipeline -> TrainFrame
  -> semantic::renderTrainFrame(TrainFrame, brightness, route mask)
  -> Phase 4M status overlay
  -> logical RgbFrame -> existing Phase 4N output bridge
```

The route renderer looks up each occupied map LED's existing canonical
`MapLed::route`. An invisible route is left black; a visible route uses the
same existing nominal route color, brightness scaling, and occupancy behavior
as before. Both STATION and SEGMENT records use this identical route check.
The renderer does not mutate `TrainFrame`, reinterpret trains, or change
collision handling.

All-visible output is byte-for-byte equivalent to the default renderer
configuration. With zero visible routes, all 302 map LEDs render black, while
the four status pixels remain owned by `display::applyStatusOverlay()` and
continue to reflect SYSTEM, NETWORK, MTA FEED, and MODE state. The separate
GPIO14 U23 indicator is not in the addressable 306-pixel `RgbFrame` and is not
filtered. In `NoData`, the operational controller's cleared frame keeps the
map black regardless of the route mask.

Route visibility is presentation policy only. It does not alter brightness,
operational display states, frame revisions, age timestamps, feed counts,
polling, provisioning, or Wi-Fi/DNS/TLS failure behavior. Re-enabling a route
can show its current `TrainFrame` immediately.

## LAN UI and API

The normal page adds a mobile-friendly **Routes to display** section with one
checkbox per canonical route label. Checked state reflects the saved mask.
Select all and Clear all are local form conveniences; Save routes submits one
explicit POST to `/config/routes`. A hidden form marker allows an empty
selection to be submitted as the valid all-hidden mask. The page uses inline
CSS/JavaScript only and adds no external dependencies or route-color swatches.

The route form accepts the exact canonical route identifiers as repeated
`route` parameters and the single `routeForm=1` marker. Unknown parameter
names or route identities, duplicate route values/markers, malformed values,
missing markers, oversized fields, and excessive argument counts return HTTP
400 without changing configuration. NVS write failure returns HTTP 500;
success redirects to `/` with HTTP 303. A form with only the marker is valid
and saves mask zero.

`GET /api/status` adds the numeric `routeVisibilityMask` and canonical
`routeCount` fields. These describe configuration only, not live train
presence. The page and API continue to report brightness independently. Route
configuration exposes no LED-enable, strip-test, current-limit, channel-map,
or raw-RGB controls.

## Host validation

The dedicated Phase 4S host suite exercises every canonical bit and map LED,
including both STATION and SEGMENT roles; one-route-hidden, one-route-visible,
multiple-hidden, all-visible, and all-hidden rendering; per-route suppression
and byte-identical re-enabling; brightness independence; status overlay
independence in Fresh, DegradedHold, DegradedPartial, StaleHold, and NoData;
and unchanged TrainFrame/freshness state during rendering.

The LAN/configuration suite covers default and missing-key behavior, Phase 4R
brightness-only migration, persistence across reconstruction, corrupt/high
bits, independent brightness and route saves, failed writes, canonical form
parsing and rejection, checked-state rendering, select/clear controls,
all-hidden submission, hardware-control absence, and route masks for all,
single-hidden, multiple-hidden, and no-visible API responses. The test runner
parses the emitted API samples with Python's JSON parser.

Full host regression result, rerun after XIAO testing:

- Phase 4S route visibility: **14,805 checks, 0 failures**
- LAN UI/configuration: **184 checks, 0 failures**
- All 14 host regression runners: **1,554,025 checks, 0 failures**

The existing RGB renderer, pipeline, operational display, provisioning,
physical-output safety, and other regressions remain unchanged and pass.

## XIAO hardware validation

On 2026-10-18, only `xiao-s3-network` was uploaded to the XIAO bench target on
COM4. The ESP32-S3 was identified by esptool, and each flashed segment reported
`Hash of data verified`. The uploaded `firmware.bin` SHA-256 was
`1eec38b7fe94ad8c7539bb08a2dd06d0ab15063149c6f059076d4554762c837f`.
No production-board environment was uploaded.

The existing Phase 4R NVS was not cleared. The Phase 4S first boot retained
brightness **64**, reconnected using the stored Wi-Fi configuration without
starting provisioning, and reported all 18 routes visible (`0x0003FFFF`).
This matches the Phase 4R brightness-only migration behavior: a missing
`route_mask` defaults to all visible.

The actual LAN page showed every canonical route exactly once and selected,
brightness 64, and physical output **DISABLED**, along with Save routes,
Select all, and Clear all controls. The actual `/api/status` response parsed
as valid JSON. It reported the mask and route count, did not contain password
or raw hardware-control fields, and reported
`physicalLedOutputEnabled: false`.

Actual route saves and post-save page/API reads produced these states:

- Initial after upgrade: 18/18 enabled; mask `262143` (`0x0003FFFF`).
- Route `2` hidden alone: POST returned 303; 17/18 enabled; mask `262142`
  (`0x0003FFFE`). Other routes remained enabled and brightness remained 64.
- Normal LAN reboot with route `2` hidden: Wi-Fi/LAN returned and mask
  `262142` persisted; brightness remained 64.
- All-hidden selection: marker-only POST returned 303; page showed 0/18
  checked and API mask `0`; brightness remained 64.
- Normal LAN reboot while all hidden: Wi-Fi/LAN returned and mask `0`
  persisted.
- Restored all-visible: page showed 18/18 checked and API mask `262143`;
  brightness remained 64.
- Final normal LAN reboot: Wi-Fi/LAN returned with mask `262143`, brightness
  64, and physical output disabled.

The route saves did not change the reported Wi-Fi connection or brightness.
These UI/API and persistence checks did not alter credentials, clear NVS, or
launch provisioning. Because physical output remained disabled, no visible
map-light change was expected or tested on hardware. STATION/SEGMENT filtering
and status-overlay independence are validated by the host suites; no production
PCB or physical status LEDs were tested.

**Hardware validation is not yet complete:** after the final reboot, the XIAO
remained connected to Wi-Fi and the LAN UI, but `timeSynchronized` stayed
false, `feedCycleState` remained `NOT RUN`, and feed counters remained 0/0.
This persisted beyond 9 minutes of uptime, including more than 3 minutes after
the user disabled Tailscale on the host. The host could resolve `pool.ntp.org`
and reach the MTA HTTPS endpoint, but this does not establish XIAO time
synchronization or feed-poll behavior. No transport policy or Wi-Fi
credentials were changed. Runtime feed/operational-state independence
therefore remains unverified on hardware, while the unchanged pipeline and
renderer behavior remains covered by host regression tests. Further hardware
validation needs an upstream network on which the XIAO can synchronize time;
do not reset NVS or alter transport policy to work around this result.

## Memory, builds, and output safety

Target object sizes, derived from the `sizeof` values in the compiled
production-network `NetworkService::begin()` diagnostic:

- `DeviceConfiguration`: 16 B in Phase 4R; **28 B in Phase 4S**
- `LanWebUi`: 508 B in Phase 4R; **508 B in Phase 4S**

The Phase 4S route preference itself is one 32-bit mask. Host rendering
generated a 5,478-byte HTML page and 705-byte status JSON response; page
capacity remains 6,144 bytes.

Clean PlatformIO builds passed for all four environments:

| Environment | Phase 4S RAM | RAM delta | Phase 4S flash | Flash delta |
|---|---:|---:|---:|---:|
| `brooklyn-s3` | 36,268 B | +32 B | 304,537 B | +108 B |
| `xiao-s3` | 35,380 B | +32 B | 289,853 B | +112 B |
| `brooklyn-s3-network` | 82,636 B | +40 B | 1,070,325 B | +5,868 B |
| `xiao-s3-network` | 81,748 B | +40 B | 1,053,505 B | +3,616 B |

Every environment keeps `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`; the network
environments alone enable `BROOKLYN_NETWORK_ENABLED=1`. The
`brooklyn-s3-network` linked-ELF symbol audit found no `led::render()`,
physical backend initializer, arming/transmit functions,
`Adafruit_NeoPixel::show()`, or Phase 4Q physical safety-controller symbols.
Logical rendering/output-bridge symbols and backend status queries remain.
Arduino's generic `neopixelWrite` helper remains linked but no project call
path to it was found. No production PCB test was performed.

The XIAO linked ELF also contains no production strip/backend/transmitter
symbols. The effective build configuration keeps
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` in all four environments. Physical
output reported disabled throughout the bench test.

The Phase 4S route/NVS, UI/API, build, host regression, and physical-output
safety checks passed. Per Joel's explicit checkpoint authorization, the
environmental time-sync/feed-polling limitation does not block checkpointing:
Phase 4S does not modify time synchronization, DNS, feed transport, TLS,
Wi-Fi association, or provisioning behavior. The XIAO is left at brightness
64 with all canonical routes visible. The Tailscale CLI reported the service
stopped after the user's action; no other host networking was changed.
Runtime feed behavior on the XIAO remains unverified and is recorded as an
environment/network limitation, not as a Phase 4S transport validation.
