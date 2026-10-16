# Phase 4N: logical LED output bridge

## Scope and architecture

Phase 4N adds a memory-only boundary between the completed Phase 4M RGB frame
and the existing LED-engine framebuffer:

```text
TrainFrame
  -> RGB route renderer
  -> Phase 4M operational status overlay
  -> completed semantic::RgbFrame
  -> Phase 4N display output bridge
  -> LED-engine logical framebuffer
  -> existing power/current protection
  -> future, separately authorized physical transmission
```

The bridge accepts the completed 306-pixel `semantic::RgbFrame`. It validates
every global-index/channel-index round trip using `led::map`, confirms the
302 map / 4 status role split and all four canonical status identities, then
stages every RGB component without interpreting or changing color, brightness,
route, or operational state. U23 is the separate GPIO14 discrete indicator;
it is not among the 306 addressable pixels and is not bridged.

`led::map` remains the sole authority for physical channel/index identity.
Phase 4N adds no LED order, channel, index, or semantic mapping table.

## Publication and brightness

The bridge stages a complete `DisplayState` frame before asking the LED engine
to publish it. The engine copies that completed frame into its inactive
framebuffer and then changes its active-frame pointer once. In the current
synchronous main-loop model, no partially copied candidate becomes active.
This atomicity statement is limited to that execution model; it is not a
thread-safety or DMA/RMT/asynchronous-consumer guarantee.

Logical brightness remains owned by the Phase 4H renderer. The current path is:

```text
TrainFrame
-> RGB route renderer (logical brightness 64/255)
-> Phase 4M status overlay
-> completed RgbFrame
-> Phase 4N byte-for-byte bridge
-> LED engine brightness 255/255
-> existing power/current protection
-> future physical transmission
```

The bridge does not apply brightness a second time. It does not render routes,
make operational-state decisions, initialize the physical backend, or
transmit physical data.

## Power/current audit

The existing limiter operates on the post-logical-brightness frame, estimates
all 306 addressable pixels using the existing 20 mA per full-scale color
component model, and applies one global scale against the unchanged 4,000 mA
firmware budget. There is no per-channel scale. The separate production
bring-up brightness cap remains 16/255.

These estimates and limits have **not** been validated against the
manufactured PCB or measured LED current. They are unchanged in Phase 4N and
must be reviewed during an explicitly authorized physical bring-up.

## Physical-output safety

All four PlatformIO environments continue to define
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. The Phase 4N path only calls the
memory-only output bridge and logical-frame publication API. It does not call
physical backend initialization, `transmitFrame()`, NeoPixel `show()`, GPIO
LED-data output, or LED_ENABLE activation. The physical backend and its
transmission implementation are unchanged.

No firmware upload was performed. The manufactured production PCB remains
physically untested.

## Validation

The host output-bridge suite covers:

- all 306 isolated pixel transfers and every address round trip
- neighboring-pixel isolation
- deterministic full-frame publication and channel-boundary pixels
- canonical 302 map / 4 status accounting and U23 exclusion from the frame
- Fresh, Degraded, NoData, and Provisioning representative RGB frames
- byte-for-byte preservation of map and status colors
- null/invalid-size publication protection and unchanged active content
- no calls into physical LED backend APIs

The expanded exhaustive transfer checks produce **476,271 checks, 0
failures**. This is above the prior 288,023-check expectation because the
isolated-transfer test verifies every pixel against every address and checks
the address round trip for each observation.

The complete regression results are:

| Check | Result |
|---|---:|
| Semantic tables | 302 map LEDs (276 STATION, 26 SEGMENT), 188 registry entities |
| Resolver | 958,614 checks, 0 failures |
| GTFS adapter | 7,383 checks, 0 failures |
| GTFS-RT | 429 checks, 0 failures |
| Previous stop | 8,098 checks, 0 failures |
| TrainFrame | 5,232 checks, 0 failures |
| RGB renderer | 71,785 checks, 0 failures |
| TrainPipeline | 4,538 checks, 0 failures |
| Feed transport | 1,209 checks, 0 failures |
| Wi-Fi provisioning | 310 checks, 0 failures |
| Operational display | 1,534 checks, 0 failures |
| Output bridge | 476,271 checks, 0 failures |

All four environments were clean-built. Results and deltas from Phase 4M:

| Environment | RAM | RAM delta | Flash | Flash delta |
|---|---:|---:|---:|---:|
| `brooklyn-s3` | 36,236 B | +928 B | 304,353 B | +724 B |
| `xiao-s3` | 35,348 B | +920 B | 289,453 B | +780 B |
| `brooklyn-s3-network` | 71,740 B | +928 B | 1,024,605 B | +716 B |
| `xiao-s3-network` | 70,852 B | +920 B | 1,009,733 B | +776 B |

These are the measured build figures; no cause is assigned to differences
from the pre-run estimates. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remained
set in every environment. No firmware upload was performed.