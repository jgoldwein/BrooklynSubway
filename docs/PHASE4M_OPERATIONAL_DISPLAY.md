# Phase 4M: operational display state and stale-data policy

**Baseline:** Phase 4L checkpoint `333682eaff40cf4a2e0b845fedb3d54a01c1c908`
(`phase-4l-wifi-provisioning`). This phase is logical/memory-only. It does not
upload firmware or validate the manufactured PCB. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`
remains set for all targets.

## Architecture and cycle boundary

The existing layers remain the single source of train semantics and route
colors:

```text
network cycle -> TrainPipeline working TrainFrame -> operational Controller
                                                   -> committed TrainFrame
                                                   -> existing RGB renderer
                                                   -> four-pixel logical status overlay
```

`TrainPipeline` owns the in-progress candidate and only finishes it after the
transport cycle has processed all downloaded feeds. `NetworkService` exposes
each finished `CycleResult` once, with its monotonic completion time; skipped
cycles leave the pipeline candidate untouched. `main.cpp` classifies usable
feeds and then gives the completed candidate to the fixed-size
`display::Controller`. The controller copies a whole `TrainFrame` only when
the policy commits it. A feed download, retry, or still-running cycle cannot
mutate the separately retained displayed frame.

A usable feed is a complete downloaded body that the pipeline decoded with
`DecodeStatus::Ok`; successfully decoded feeds with no train positions still
count as usable. A body with a decode failure does not count toward a complete
data set. A complete transport cycle with decode failures is treated as
partial if other feeds decoded successfully, or as no usable feeds if none
did. This does not change transport outcomes or timeout policy.

The controller retains only its current `TrainFrame` and timing/state metadata.
No raw GTFS-RT protobuf is cached; the existing single reusable PSRAM
transport buffer and current pipeline candidate are unchanged. Route colors
continue to come from `renderTrainFrame`; there is no duplicate route-color
table, animation, fade, interpolation, or simulation.

## States and frame policy

`display::State` is separate from `network::State`, transport-cycle outcome,
and provisioning mode:

| State | Meaning |
|---|---|
| `WaitingForData` | No usable feed cycle has completed yet. Logical map is blank. |
| `Fresh` | Latest committed candidate came from a fully decoded COMPLETE cycle. |
| `DegradedHold` | A PARTIAL cycle arrived within the complete-frame grace period; retain the previous frame. |
| `DegradedPartial` | A usable PARTIAL candidate is displayed. |
| `StaleHold` | Keep the displayed frame temporarily while data is unavailable or no usable candidate has arrived. |
| `NoData` | No usable frame is available, or the prior one reached the hard stale limit. |

Named timing constants are `kPartialGraceMs = 90000` (90 seconds) and
`kHardStaleLimitMs = 300000` (5 minutes).

- **Startup COMPLETE:** copy the candidate, enter `Fresh`, and record both the
  latest commit time and last-complete time.
- **Startup PARTIAL with usable feeds:** copy immediately and enter
  `DegradedPartial`; useful data is not withheld just because startup was
  incomplete.
- **Startup NO FEEDS:** stay blank and enter `NoData`.
- **COMPLETE at any time:** atomically replace the displayed frame, enter
  `Fresh`, and reset both monotonic timestamps.
- **PARTIAL within grace:** if the last COMPLETE finished no more than 90
  seconds ago, hold the displayed frame byte-for-byte and enter
  `DegradedHold`. At exactly 90 seconds it is still held; after that the
  current usable PARTIAL candidate is committed and state becomes
  `DegradedPartial`.
- **Prolonged PARTIAL:** once the complete-frame grace has expired, commit
  each usable partial candidate normally and refresh the last-usable-commit
  time. If partial candidates stop arriving for more than the grace period,
  state becomes `StaleHold`.
- **NO FEEDS with a displayed frame:** enter `StaleHold` without immediately
  clearing the frame. Skipped cycles (for example Wi-Fi down, time not ready,
  or buffer unavailable) do not masquerade as feed cycles; normal loop aging
  still applies.
- **Hard stale:** at exactly 300 seconds since the latest usable frame commit,
  clear the logical `TrainFrame` and enter `NoData`. The existing RGB renderer
  then blanks map pixels; status pixels are overlaid afterward. A later usable
  PARTIAL or COMPLETE cycle automatically recovers the display.

The partial-frame hold is based on time since the last COMPLETE cycle. Hard
staleness and idle aging are based on time since the most recent usable frame
commit, including a prolonged-operation partial commit.

## Timebase and diagnostics

All freshness calculations use `millis()`-equivalent unsigned 32-bit
monotonic milliseconds. Elapsed time is calculated by unsigned subtraction
(`now - then`); no UTC/SNTP timestamp participates in the state machine. The
90-second and 300-second limits are far below the uint32 half-range, and tests
exercise them across rollover.

Serial output is limited to state transitions, for example
`Display: Fresh -> DegradedHold (partial cycle, holding prior frame; data age
30000 ms)` and `Display: StaleHold -> NoData (hard stale limit reached; data
age >= 300000 ms)`. RGB arrays are not dumped.

## Logical status pixels

The overlay changes only the four status pixels resolved through
`led::map::statusLedAddress` and the canonical global-index lookup. The route
renderer still emits black for status LEDs and never paints a train route
color there. The 302 map pixels are unchanged by the overlay. U23 on GPIO14 is
a separate discrete LED and is not part of the 306-pixel `RgbFrame`.

Status colors are nominal RGB before the shared logical brightness scale
(default 64):

| Indicator | Status | Color |
|---|---|---|
| SYSTEM | Self-check and core startup healthy | Green `(0,255,0)` |
| SYSTEM | Detected self-check or core startup failure | Red `(255,0,0)` |
| NETWORK | Associated and service ready | Blue `(0,0,255)` |
| NETWORK | Station disconnected/associating | Red `(255,0,0)` |
| NETWORK | Associated but waiting for time sync | Amber `(255,128,0)` |
| NETWORK | Provisioning portal active | Cyan `(0,255,255)` |
| NETWORK | Network feature disabled | Off `(0,0,0)` |
| MTA FEED | No feed attempt yet | Off `(0,0,0)` |
| MTA FEED | `Fresh` | Green `(0,255,0)` |
| MTA FEED | `DegradedHold`, `DegradedPartial`, or `StaleHold` | Amber `(255,128,0)` |
| MTA FEED | `NoData` after an attempt | Red `(255,0,0)` |
| MODE | Normal operation | Off `(0,0,0)` |
| MODE | Provisioning/recovery portal | White `(255,255,255)` |

Status colors use the existing integer `scaleBrightness` function and the
same brightness as map routes. These are logical RGB values only until a
separate future phase deliberately enables physical output.

Provisioning suspends MTA polling per Phase 4L, does not clear credentials or
the currently committed map, and sets NETWORK cyan / MODE white. Staleness
continues to age in the main loop. After provisioning, station operation and
polling resume; a usable cycle recovers the map. Wi-Fi loss likewise leaves
credentials and recovery policy alone; the network indicator reflects the
link state, the display ages to `StaleHold` and then `NoData`, and a later
usable feed cycle recovers it. DNS, Internet, TLS, and MTA errors remain
transport failures and do not enter provisioning.

## Hardware safety

The operational controller, RGB renderer, and status overlay only manipulate
fixed-size memory. Phase 4M does not call NeoPixel `show()`, physical LED
setters, or `LED_ENABLE`; it does not change LED GPIOs, physical channel/index
order, semantic assignments, manufacturing data, or transport timeout values.
The existing production-backend initialization call in `main.cpp` was removed
so normal startup cannot send its prior physical black frame during this
memory-only phase. The physical backend implementation itself is unchanged.
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains mandatory; the production PCB
remains physically untested.

## Memory and validation

- `sizeof(display::Controller)`: **344 bytes**, fixed-size and heap-free.
- Separate committed logical `RgbFrame`: **918 bytes**.
- Added persistent display state/output storage: **1,262 bytes**, plus the
  small completed-cycle handoff metadata in `NetworkService`; final PlatformIO
  RAM deltas include all changes. The production build's net RAM delta also
  reflects removal of its former startup physical-backend initialization path.
- `TrainFrame`: 328 bytes. There is no raw-protobuf cache added.
- Host tests exercise startup COMPLETE/PARTIAL/NO FEEDS, atomic hold and
  replacement, just-before/exact/after grace and stale boundaries, collision-preserving copies,
  recovery, polling isolation, provisioning overlay, status/map pixel
  separation, brightness, repeated transitions, DNS-style no-feed recovery,
  and uint32 rollover.

### Host validation

All requested checks passed:

| Check | Result |
|---|---:|
| Semantic table validation | 302 map LEDs (276 STATION, 26 SEGMENT), 188 registry entities |
| Resolver | 958,614 checks, 0 failures |
| GTFS adapter | 7,383 checks, 0 failures |
| GTFS-RT | 429 checks, 0 failures; all eight feed fixtures passed |
| Previous stop | 8,098 checks, 0 failures |
| TrainFrame | 5,232 checks, 0 failures |
| RGB renderer | 71,785 checks, 0 failures |
| TrainPipeline | 4,538 checks, 0 failures |
| Feed transport | 1,209 checks, 0 failures |
| Wi-Fi provisioning | 310 checks, 0 failures |
| Operational display | 1,534 checks, 0 failures |

### PlatformIO size and Phase 4L deltas

| Environment | RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| `brooklyn-s3` | 35,308 B | +1,044 B | 303,629 B | -2,852 B |
| `xiao-s3` | 34,428 B | +1,304 B | 288,673 B | +2,144 B |
| `brooklyn-s3-network` | 70,812 B | +1,072 B | 1,023,889 B | -1,396 B |
| `xiao-s3-network` | 69,932 B | +1,304 B | 1,008,957 B | +2,200 B |

The non-network environments remain network-disabled; the `*-network`
environments retain the Phase 4L opt-in. All four retain
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. No firmware upload was performed.
The production PCB remains physically untested.
