# Phase 4Q — software-only black-first output interlock

## Scope and safety status

The Phase 4P audit found that the existing initialization-time black `show()`
occurred while the level shifters were disabled. It could not establish that
the LED chain had received a clearing frame. The audit also established that
powered WS2812-class LEDs may retain their previously latched visible colors
across an MCU reset.

Phase 4Q adds a tested interlock for any future authorized physical-output
path. It does not connect that path to the normal `main.cpp` display flow.
The current production path remains memory-only.

Every PlatformIO environment still defines
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. No firmware was uploaded, and no
physical LED or production-PCB test was performed. This work is not
authorization to enable physical output.

If the guarded PCB output path is separately authorized in the future, it
affects GPIO1 (`LED_ENABLE`) and the eight existing addressable-LED data pins:
GPIO4, GPIO5, GPIO6, GPIO7, GPIO10, GPIO11, GPIO12 and GPIO13. The pin map,
manufacturing data, logical map, and output policy were not changed.

## Required arming sequence

`brooklyn::led::safety::Controller` in
`include/physical_output_safety.h` and `src/physical_output_safety.cpp` uses
the states `Disabled`, `BackendReady`, `ClearingBlack`, `Armed`, and `Fault`.
It enforces this sequence:

1. Initialize with LED_ENABLE LOW.
2. Initialize the backend and hold all eight data pins LOW.
3. Construct an explicit all-zero frame covering all 306 addressable pixels
   (302 map positions and 4 status positions; U23 is separate) and pass it
   through the existing production power limiter.
4. Reject arming unless the limited frame is still entirely black.
5. Raise LED_ENABLE and synchronously transmit that black frame.
6. Enter `Armed` only after the transmission callback reports completion.
7. Permit later frames only while in `Armed`.

Initialization deliberately does not call `show()` before enable. Backend
initialization configures the driver and GPIO state; the required black frame
is sent only after LED_ENABLE is raised. The installed NeoPixel driver is
initialized by `begin()` before arming, and the clearing transmission uses its
existing `show()` operation. The black frame is limited using the existing
16/255 cap and 4,000 mA budget. Black remains black through that production
limiter. The limiter policy and logical renderer brightness were not changed.

Disarm and failures drive LED_ENABLE LOW and the data pins LOW. An unsuccessful
black preparation, enable, or black transmission never enters `Armed`; a
retry requires a new black-first arm sequence. A regular frame transmission
failure also disarms output and requires a new black-first arm sequence.
Explicit disarm clears `Armed`; controller construction and reconstruction
start in `Disabled`.

The controller itself has no heap state. The temporary 306-pixel limited
black frame occupies 918 bytes on the stack during arming. The installed
NeoPixel/RMT `show()` path is synchronous, but its API returns no electrical
acknowledgment: success means the software callback completed, not that every
LED's reception or latch was independently measured.

## Reset-retained LED state

Powered WS2812-class LEDs may retain their previously latched visible colors
across an MCU reset. LED_ENABLE LOW / level-shifter disable prevents new data
from reaching the LED chain but does not erase already-latched LED state.
Therefore Phase 4Q guarantees that the first newly transmitted frame after
enabling the physical path is black. It does **not** guarantee that the LEDs
were physically dark during the MCU-reset interval before that black frame
reaches them. This is a software ordering guarantee, not electrical
verification on the manufactured PCB.

## Validation

All 12 host/regression runners passed with zero failures. Exact checks:

- Physical-output safety: 3,630 checks. The count increased from 2,915
  because final checkpoint coverage adds explicit color-transmission-failure
  disarm and fresh-black re-arm assertions, including a second full
  all-pixel black-frame verification.
- Output bridge: 476,271 checks. The supplied expected count was 288,023;
  the current unmodified runner reports 476,271, with zero failures.
- Operational display: 1,534 checks.
- RGB renderer: 71,785 checks.
- Train pipeline: 4,538 checks.
- TrainFrame: 5,232 checks.
- Previous stop: 8,098 checks.
- Feed transport: 1,209 checks.
- Wi-Fi provisioning: 310 checks.
- Resolver: 958,614 checks.
- GTFS adapter: 7,383 checks.
- GTFS-RT decoder: 429 checks.

Semantic validation confirmed 302 map LEDs (276 STATION and 26 SEGMENT) and
188 registry entities. The safety suite verifies cold Disabled state,
pre-initialization and pre-arm rejection, full 306-pixel all-zero clearing
across all eight channel regions (including status pixels), NoData not being
used as the forced clear, exact event order, Armed only after black
transmission completion, repeated normal frames while Armed, black and color
transmission failures, disarm/re-arm, reset reconstruction, compile-time-zero
behavior, production limiter preservation, and the controller's fixed
no-heap state.

Clean builds passed for all four environments:

| Environment | RAM | Flash |
|---|---:|---:|
| `brooklyn-s3` | 36,236 B | 304,309 B |
| `xiao-s3` | 35,348 B | 289,585 B |
| `brooklyn-s3-network` | 71,740 B | 1,024,569 B |
| `xiao-s3-network` | 70,852 B | 1,009,865 B |

Compared with the Phase 4N baseline, the deltas are:

| Environment | RAM delta | Flash delta |
|---|---:|---:|
| `brooklyn-s3` | +8 B | +128 B |
| `xiao-s3` | +8 B | +304 B |
| `brooklyn-s3-network` | +8 B | +116 B |
| `xiao-s3-network` | +8 B | +284 B |

The production build uses the zero permission flag. Its linked ELF contains
none of the physical renderer, backend initialization/arming/transmission,
interlock implementation, strip-construction, or NeoPixel `show()` symbols.
The normal `main.cpp` remains memory-only. The interlock host tests verify
that a compile-time-disabled controller performs no callback operations.
Current/power policy, route colors, logical mapping, pin assignments, Wi-Fi
policy, transport timeouts, generated semantics, and manufacturing data are
unchanged.

The production PCB remains physically untested. No firmware upload, physical
LED write, or manufacturing-file change was made. All current estimates and
brightness caps remain unvalidated on production hardware. No additional
firmware phase is started from this checkpoint.
