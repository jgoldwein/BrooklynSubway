# Phase 4P: physical LED output read-only audit

## Scope and evidence

This is a source/build audit only. No firmware source, hardware, or output
authorization was changed. No firmware was uploaded and no physical output
test was performed. The production PCB remains physically unvalidated.

Evidence reviewed:

- production and XIAO PlatformIO configuration in [platformio.ini](../platformio.ini)
- canonical topology and order in [board_pins.h](../include/board_pins.h) and
  [led_map.cpp](../src/led_map.cpp)
- frame publication and rendering in [display_output.cpp](../src/display_output.cpp)
  and [led_engine.cpp](../src/led_engine.cpp)
- GPIO startup in [hardware.cpp](../src/hardware.cpp)
- backend gate and transmission in [led_backend.cpp](../src/led_backend.cpp)
- power model in [led_power.h](../include/led_power.h) and [led_power.cpp](../src/led_power.cpp)
- RGB generation in [rgb_renderer.cpp](../src/rgb_renderer.cpp) and the status
  overlay in [operational_display.cpp](../src/operational_display.cpp)
- the locally installed Adafruit NeoPixel 1.12.3 and Arduino-ESP32 framework
  2.0.17 source, plus the production build object and linked ELF symbols

## Call path

Current logical-frame path:

```text
NetworkService completed-cycle handoff
  -> main loop
  -> OperationalDisplay committed TrainFrame
  -> semantic::renderTrainFrame()
  -> display::applyStatusOverlay()
  -> display::output::publish()
  -> validate/copy completed 306-pixel RgbFrame
  -> led::publishFrame()
  -> copy inactive engine buffer; swap active framebuffer pointer
```

The production physical-render path, if it were called, is:

```text
active LED-engine framebuffer
  -> led::render()
  -> power::limitFrameToBudget(frame, 306, engineBrightness,
                               productionCap=16)
  -> local limited DisplayState[306]
  -> backend::transmitFrame(limitedFrame, 306)
  -> require initialized && armed && physical-output permission
  -> each channel: setPixelColor() for that channel's pixels
  -> Adafruit_NeoPixel::show()
  -> drive that channel's data pin LOW
```

`main.cpp` currently calls `display::output::publish()` but does not call
`led::render()`, `initializePhysicalLedBackend()`, or `armLedOutputs()`.
The only render calls are in XIAO-only demonstration/self-test code. In the
production build, linked-ELF symbol inspection retained the logical bridge
and status queries but did not retain `led::render()`, backend initialization,
arming, transmission, or Adafruit `show()`. The `led_backend.cpp.o` production
object was nevertheless compiled and contains backend function definitions
and unresolved Adafruit `begin/clear/show` references. Thus the backend is
compiled as an object, while the current production firmware entry path does
not reach physical initialization or transmission.

## Compile-time output gate

`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` is set in both base environments in
[platformio.ini](../platformio.ini): `brooklyn-s3` and `xiao-s3`. The two
network environments extend those bases and inherit the zero value.

The production backend code is **not wholly removed by this macro**:

- `led_backend.cpp` requires the macro for PCB builds and rejects values other
  than 0 or 1.
- `armLedOutputs()` contains the `#if ... == 1` branch containing the only
  firmware `digitalWrite(LED_ENABLE, HIGH)`. With value 0, that branch is
  excluded; the remaining PCB branch disarms and reports refusal.
- `physicalLedOutputPermissionEnabled()` returns false for value 0.
- `transmitFrame()` remains defined in source and checks initialized, armed,
  and permission before processing pixels. Under the current zero value,
  permission is false; it cannot reach the channel loop through that API.
- `initializePhysicalLedBackend()` has no `BROOKLYN_ALLOW...` guard. If
  explicitly called in a flag-zero PCB build, it can construct NeoPixel
  objects, initialize data GPIOs, and call `show()` while LED_ENABLE is low.
  It does not arm the level shifters. That function currently has no runtime
  caller and is absent from the linked production ELF.
- The backend object is compiled, but current production linked-ELF inspection
  shows no linked `led::render`, `initializePhysicalLedBackend`,
  `armLedOutputs`, `transmitFrame`, or Adafruit `show` symbols. Backend status
  queries are linked. This is stronger evidence for the current binary than
  source-only reachability, but not a permanent guarantee if call sites or
  linker settings change.

Consequently the strongest supported conclusion is: the current build cannot
raise LED_ENABLE through the guarded firmware arm function, and the linked
production image has no active renderer/initializer/transmitter call path.
The macro is not a complete compile-out of backend source, and should not be
treated as authorization to call initialization or to change the build flag.

## LED_ENABLE and board-level inhibit

- Production `LED_ENABLE` is GPIO1 in [board_pins.h](../include/board_pins.h).
- It is active HIGH: GPIO1 HIGH turns on Q1, which asserts the active-low
  `LED_OE_N` and enables the U14/U15 level-shifter outputs.
- `hardware::beginSafe()` explicitly writes GPIO1 LOW, configures it OUTPUT,
  then writes LOW again, before other peripheral setup.
- `led_backend::disarmLedOutputs()` also writes it LOW. The only code write
  HIGH is inside the arm function's compile-time-enabled branch.
- Under the current flag-zero production build, the normal setup path drives
  GPIO1 LOW; it does not initialize the eight LED data GPIOs. They are
  configured by Adafruit `begin()` only if the currently uncalled physical
  backend initializer runs.
- The established schematic architecture is R59 as the LED_ENABLE pull-down,
  Q1 controlling `LED_OE_N`, and R58 pulling `LED_OE_N` up. This supports an
  output-disabled reset/default state at schematic level. It is schematic
  evidence only; none of these electrical behaviors has been physically
  validated on the manufactured PCB.

## Canonical eight-channel audit

GPIO/count come from the production board pin definitions. Global ranges,
channel offsets, and endpoint designators below are derived from the sole
canonical `led::map` counts, offsets, and designator array in
[led_map.cpp](../src/led_map.cpp); the bridge adds no alternative map.

| Channel | GPIO | LEDs | Global range | First canonical pixel | Last canonical pixel |
|---|---:|---:|---:|---|---|
| CH1 | 4 | 37 | 0–36 | LED42 | LED32 |
| CH2 | 5 | 38 | 37–74 | LED208 | LED7 |
| CH3 | 6 | 37 | 75–111 | LED226 | LED232 |
| CH4 | 7 | 37 | 112–148 | LED220 | LED178 |
| CH5 | 10 | 42 | 149–190 | LED190 | LED187 |
| CH6 | 11 | 37 | 191–227 | LED194 | LED83 |
| CH7 | 12 | 38 | 228–265 | LED198 | LED253 |
| CH8 | 13 | 40 | 266–305 | LED252 (SYSTEM) | LED8 |
| **Total** |  | **306** | **0–305** |  |  |

The canonical role split is 302 map pixels and four status pixels at CH8
indices 0–3. U23 is the separate GPIO14 discrete Avi indicator and is not an
addressable pixel or part of these counts. The source-derived counts,
offsets, endpoint designators, and roles agree; no mapping conflict was found.

## Backend initialization and first-frame state

For a future explicit production initialization, `productionStrips()` creates
eight Adafruit NeoPixel objects using each configured channel count and GPIO,
with `NEO_GRB + NEO_KHZ800`. Constructors allocate per-strip pixel storage;
the initializer checks each `getPixels()` result before continuing. The
aggregate RGB payload is 306 × 3 = 918 bytes, separate from object and driver
state.

For each strip, initialization calls `begin()` (sets its data pin OUTPUT and
LOW), `clear()`, then `show()`. It writes all channel pins LOW again and marks
the backend initialized/disarmed. `show()` therefore emits an all-black
waveform at MCU GPIO pins during initialization, but this occurs after
`disarmLedOutputs()` has held GPIO1 LOW, so the level shifters should be
disabled and the black waveform should not reach LED inputs. The subsequent
arm path again disarms/forces data pins LOW and only then writes GPIO1 HIGH
when the build flag is 1.

The installed Adafruit driver uses ESP32 RMT for `show()`. In its ESP-IDF 4
path it reserves an RMT channel, configures/installs the driver, writes the
pixel sample, waits up to 100 ms for TX completion, uninstalls the RMT driver,
and releases the channel before returning. It is synchronous in this
configuration; it does not retain the caller's pixel pointer after `show()`
returns. Each strip's pixel storage is reused; an RMT channel is reserved for
each show call and released afterward. The driver also creates a temporary
RMT waveform array for each `show()`.

All LED-engine frame buffers have static zero initialization. `led::begin()`
clears its active buffer and sets engine brightness to 255; the other buffer
also begins zeroed. The first main-loop display update renders the initially
empty operational frame, applies the status overlay, and publishes that
complete frame before any usable feed is required. Thus a first published
logical frame can be black map pixels plus status indicators, not a validated
route frame. No production physical render is currently called.

**Dark-start limitation for future output:** initialization's black `show()`
occurs while the output-enable buffers are disabled, so it does not prove that
the addressable LEDs received a black/reset frame. The current arm API does
not require an explicitly validated black frame or frame-valid state before
raising LED_ENABLE.

A powered WS2812-class LED retains its previously latched RGB state across an
MCU reset until the LED itself loses power or receives and latches a new RGB
frame. Driving LED_ENABLE LOW / disabling LED_OE_N blocks new data; it does
not erase that already-latched state. Therefore a reset while external LED
power remains present may leave the old display visible temporarily, until
firmware can regain control and transmit another frame. The firmware cannot
guarantee that powered LEDs are physically dark throughout reset.

The software guarantee to implement before physical output is authorized is
that a known-black frame is the first frame successfully transmitted after
the physical data path is enabled, before any non-black frame is permitted.
The schematic pull-down/OE arrangement and low idle data should inhibit new
data pulses during startup, but neither that nor a controlled dark startup is
physically verified. Do not infer black receipt from the current
`initializePhysicalLedBackend()` log message.

## Limiter and brightness ownership

- Logical route colors are scaled in `semantic::renderTrainFrame()` by the
  configured brightness (currently 64/255). The status overlay independently
  applies the same logical brightness to its four status pixels. The bridge
  copies these 918 RGB bytes without scaling or color changes.
- LED-engine brightness starts at 255/255. The production `led::render()`
  path supplies all 306 active pixels to `limitFrameToBudget()` with the
  production cap 16/255. The limiter caps requested engine brightness, then
  applies its calculated global scale to every component into a separate
  local `limitedFrame`.
- `estimateFrameCurrentMa()` sums every red, green, and blue component across
  the supplied pixel count, applies the 20 mA-per-full-scale-component
  assumption and brightness factor, and rounds upward.
- `FIRMWARE_LED_CURRENT_BUDGET_MA` is 4,000 mA and
  `PRODUCTION_BRINGUP_BRIGHTNESS_CAP` is 16 (16/255), both in
  [led_power.h](../include/led_power.h). The budget scale is global; there is
  no per-channel limiter.
- The future physical frame differs from the Phase 4N staged frame: it is
  capped/scaled in the local `limitedFrame` and passed to the backend; the
  active staged buffer is not modified. At the current logical brightness 64
  followed by the 16/255 production cap, each already-rendered component is
  additionally reduced by the cap and then possibly by the global power scale.

The model, component-current assumption, 4 A budget, 16/255 cap, supply
behavior, and LED load must be compared with measurements on the real board
before any of those policies are changed. They are not hardware-validated.

## Buffer publication and transmission concurrency

Phase 4N fully copies into the inactive 306-pixel engine buffer and changes
the active pointer once. In the current synchronous main-loop path, the
publisher and consumer are not concurrent. If `render()` is called, it first
copies and limits the active buffer into its own local frame; channel `show()`
calls wait for RMT TX completion before the next channel and before the call
returns. The RMT driver is uninstalled before returning, so the current
transmission does not retain that local frame asynchronously.

This is not a future concurrency guarantee. If a task/ISR/async transmitter
consumes the active buffer while another context publishes, a subsequent
pointer swap could make the buffer currently being read become the next
inactive destination. `publishFrame()` could then overwrite it during a
consumer read. The current two-buffer pointer-swap design must be re-audited
and synchronized or otherwise redesigned before adding asynchronous
transmission, DMA/RMT pointer retention, or concurrent frame publication.

## Proposed first physical-output test (design only)

No stage below is authorized or executed by this audit. Phase 4O's USB-only
checklist is Stage 0; all later stages require separate review/authorization.

0. Complete the Phase 4O USB-only validation with external 5-V/barrel power
   disconnected; keep `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`.
1. After schematic/power review, connect external LED supply with firmware
   physical output still disabled and GPIO1 LOW. Measure rails and observe
   current/illumination. Use an approved current-limited source and do not
   infer an experimental current limit from the unvalidated firmware model.
2. Require explicit approval before any build changes
   `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT`; keep GPIO1 LOW during this review.
3. Before any non-black output, resolve the dark-start limitation above.
   Prepare a known all-black frame and arrange a reviewed, controlled procedure
   that sends black through the enabled level shifters before allowing any
   colored frame. Verify expected low idle/data and current.
4. Test one status pixel only at the minimum useful logical color, with the
   existing 16/255 cap retained and the current estimate treated as advisory.
   Measure actual current; do not use full white or all-pixel patterns.
5. Verify that the single illuminated identity is the expected CH8 status
   pixel, then disarm and confirm it goes dark. Do not infer identity from
   logical indices alone; correlate the designator/silkscreen.
6. Test one pixel per channel, one channel at a time, at low brightness;
   verify the exact first/known pixel for each channel against the canonical
   map.
7. Only after all single-pixel checks pass, test a small, low-brightness
   pattern with measured current and an explicit stop/review checkpoint.
8. Consider a normal operational display only after a separate review of
   electrical results, startup behavior, measured power, mapping, and
   transmission concurrency.

## Immediate stop conditions

Stop the test and return LED_ENABLE LOW / remove LED power when safe if any of
the following occurs:

- unexpected LED illumination
- wrong LED or channel lights, multiple LEDs light when one was expected, or
  physical order disagrees with the canonical mapping
- current exceeds the reviewed limit, unexpected voltage sag, or component
  heating
- brownout/reset, USB disconnect/reset, watchdog, or data corruption
- LED_ENABLE state disagrees with the expected firmware state
- any output occurs while `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`
- unexplained current draw or a mapping/self-check failure

Do not continue by changing GPIOs, mapping, power limits, brightness cap, or
the output gate without a separate reviewed change.

## Findings that block physical-output authorization

1. No controlled-black-before-color invariant is enforced by the current arm
   API; the black initialization waveform is sent while level-shifter outputs
   are disabled. Resolve and test the dark-start sequence before permitting
   physical output.
2. The production main path does not currently initialize, arm, or render
   physical output. Any future wiring of those calls needs its own reviewed
   sequence and validation; do not enable output just by changing the build
   macro.
3. The power/current policies and actual board rails remain unmeasured.
4. The production PCB is not physically validated. Schematic-level GO applies
   only to USB-only initial bring-up with external 5 V disconnected.

Phase 4P is read-only analysis/documentation. It is intentionally uncommitted,
untagged, and unpushed for review.
