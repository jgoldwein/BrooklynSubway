# Phase 4H — Logical RGB train-frame renderer

Status: implemented, software-only, **uncommitted**. Baseline: commit `ea5be36`
(tag `phase-4g-train-frame`).

**No physical LED output exists yet.** Nothing in Phase 4H calls `show()`,
`setPixelColor()` or any other physical setter/output API, instantiates a LED
strip, or touches GPIO, `LED_ENABLE` or U23 (GPIO14). `RgbFrame` is just
memory. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT` remains `0`.

> The brightness value used by Phase 4H is a logical rendering scale and has
> not yet been validated as a physical board power/current limit.

## Architecture boundary

    GTFS-RT -> state/previous-stop -> adapter -> resolver
        -> TrainFrame (Phase 4G, occupancy counts)
        -> RgbFrame  (Phase 4H, this phase, in memory)
        -> FUTURE temporal renderer -> FUTURE physical LED driver

The renderer is a pure function of `TrainFrame` + `RenderConfig`. It has no
knowledge of networking, GTFS, trip identity or LED drivers.

## Files

- `include/rgb_renderer.h`, `src/rgb_renderer.cpp` — firmware-capable renderer.
- `tools/host_tests/rgb_renderer_test.cpp`, `tools/run_rgb_renderer_tests.py` — host tests.
- `tools/host_tests/gtfs_rt_diag.cpp` — new `--rgb [--brightness N]` live diagnostic (host only; uses the firmware renderer, no duplicate colour table).
- `tools/phase4e_host.py` — `src/rgb_renderer.cpp` added to the host sources.

## RGB frame representation

The existing generic POD `brooklyn::led::Rgb {red, green, blue}` from
`led_types.h` is reused (`sizeof` = 3, `static_assert`ed). It has no hardware
or library coupling, so no new RGB type and no LED-library dependency was added.

    struct RgbFrame    { Rgb pixels[306]; };   // index = canonical led::map global index
    struct RenderConfig{ uint8_t brightness = 64; };
    struct RenderResult{ renderedLeds, stationLeds, segmentLeds, rejectedLeds, occupiedStatusLeds };

`sizeof(Rgb)` = 3, `sizeof(RgbFrame)` = 918, `sizeof(RenderConfig)` = 1,
(`sizeof(TrainFrame)` = 328). No second indexing scheme exists; no heap is used.
The colour byte order of the physical strips is not modelled here (no
colour-order assumption); a future driver must handle it.

## Route colours (nominal, before brightness)

Defined once in `nominalRouteColor(Route)` (`src/rgb_renderer.cpp`).

| Routes | RGB |
|---|---|
| 2, 3 | `#EE352E` |
| 4, 5 | `#00933C` |
| A, C | `#0039A6` |
| B, D, F, M | `#FF6319` |
| G | `#6CBE45` |
| J, Z | `#996633` |
| L | `#A7A9AC` |
| N, Q, R | `#FCCC0A` |
| S | `#808183` |
| `Route::None` / unknown | black `(0,0,0)` |

Routes sharing a family colour deliberately share one value. The renderer sees
only semantic `Route::S`; GTFS aliases H and FS are normalised upstream.

## Brightness model

Global integer scale 0..255: `scaled = (component * brightness + 127) / 255`
per component. 255 reproduces the nominal colour exactly, 0 gives black, and
the default is **64** (about 25% of nominal). There is no floating point. This is a logical
scale only and makes no power or current claim (see the note above).

## Collision policy

`occupancy == 0` -> black. `occupancy >= 1` -> the semantic route colour at the
configured brightness. Occupancy 1, 2, 3 or 255 render identically; no additive
brightness, colour mixing, blinking, pulsing, fading or temporal
multiplexing. Collision counts remain in `TrainFrame` diagnostics; the renderer
never modifies its input.

## Status LEDs and U23

All four status LEDs are always `(0,0,0)`. If a corrupt `TrainFrame` marks a
status LED occupied, that entry is left black and counted in
`RenderResult::rejectedLeds` / `occupiedStatusLeds`; it is never painted.
U23 is a discrete GPIO14 LED; it does not appear in `TrainFrame` or `RgbFrame`.

## Corrupt-input behaviour

For each occupied index the renderer re-validates: a semantic record exists
(status LEDs have none), `led::map::physicalLed` reports role Map, and the
global order, channel, channel index and designator agree with `led::map`;
then `Route != None`. Any failure leaves the pixel black and increments
`rejectedLeds`. Rendering never reads outside the 306-entry array.

## Host tests

`python tools/run_rgb_renderer_tests.py`: **71,785 checks, 0 failures**.
Covers: empty frame; every route's exact nominal RGB; route-family equality
and inequality; brightness 255 exact, 0 black, 64 deterministic (exhaustive over
all 256x256 component/brightness pairs against the integer rule); one station
and one segment LED; each of the 302 map LEDs rendered individually (exactly
one non-black pixel, status LEDs black); all 302 together (276 station, 26
segment); occupancy 1/2/3/255 identical; `TrainFrame` byte-for-byte and its
counters unchanged; repeated rendering byte-identical (also over a pre-dirtied
output); corrupt occupied status LEDs rejected and black; `Route::None`
and an out-of-range route value black; semantic table hash unchanged.

## Live snapshot (host only)

**Single live snapshot**, MTA feed header timestamps 2026-10-14
09:13:08–09:13:17 America/New_York (read from the feed headers), 8 feeds,
default brightness 64:

- 1,623 entities, 877 merged trips, 629 supported-route trips
- ResolvedStation 138, ResolvedSegment 1, Unrepresented 18, UnsupportedRoute 248,
  UnknownStop 164, Skipped 298, OperationalStop 10, Invalid 0, InsufficientPosition 0
- 139 resolved trains -> frame: 120 occupied LEDs (station 119, segment 1),
  18 collision LEDs, 19 excess trains, max occupancy 3
- rendered: 120 (station 119, segment 1); non-black RGB pixels 120 (== occupied);
  status LEDs non-black 0; renderer rejected entries 0

Representative LEDs from the diagnostic: F (orange) LED41 Ditmas Av, occupancy 2
(collision), nominal `#FF6319` -> scaled (64,25,6); G (green) LED229 Church Av
`#6CBE45` -> (27,48,17); R (yellow) LED20 25 St `#FCCC0A` -> (63,51,3);
5 (green) LED226 `#00933C` -> (0,37,15); 2 (red) LED145 Beverly Rd `#EE352E` ->
(60,13,12); S LED185 `#808183` -> (32,32,33); C (blue) LED119 Hoyt-Schermerhorn
`#0039A6` -> (0,14,42); J LED257 `#996633` -> (38,26,13); L LED80 Graham Av
`#A7A9AC` -> (42,42,43). The one segment train in this snapshot is counted in
the frame and rendered. One snapshot does not demonstrate every route family or
segment geometry; the rest is covered by host tests.

## Memory

- Renderer object (`rgb_renderer.cpp.o`): 691 B text, 0 data, 0 bss.
- Static RAM when instantiated: `RgbFrame` 918 B (plus the 328 B `TrainFrame`).
- Forced-link measurement (temporary reference with one static `TrainFrame` and
  `RgbFrame` and a call to `renderTrainFrame`, since removed):
  brooklyn-s3 RAM 20,456 -> 21,712 B (+1,256 B), flash +6,620 B;
  xiao-s3 RAM 25,972 -> 27,228 B (+1,256 B), flash +5,804 B.
  The RAM figure is the two frames plus a few bytes of reference-harness state.
  The flash figure includes the harness and the semantic lookup/table code the
  renderer depends on, so it is **not** the renderer code size alone (691 B).
- Clean default firmware (renderer not referenced): brooklyn-s3 RAM 20,456 B / flash 286,029 B; xiao-s3 RAM 25,972 B / flash 265,065 B. Flash is 40 B above the Phase 4G figures on both targets; that difference was not investigated.

## Remaining before LED output

Temporal renderer (collision display, persistence), status-LED policy, colour
order and gamma handling in a driver, a validated power/current limit and
brightness cap, and any physical output.
