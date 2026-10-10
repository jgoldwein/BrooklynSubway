# Brooklyn Active Subway Map Firmware

Initial firmware scaffold for the manufactured Brooklyn Active Subway Map V1.1 PCB.

## Start here

1. Read `PROJECT_CONTEXT.md`.
2. In VS Code with the official OpenAI Codex extension, open this folder.
3. Codex automatically uses the root `AGENTS.md` as project instructions.
4. First Codex prompt:

   `Read AGENTS.md and PROJECT_CONTEXT.md. Summarize the manufactured hardware constraints and do not change any files yet.`

## PlatformIO targets

- `xiao-s3`: travel/bench compiler target (default initially).
- `brooklyn-s3`: manufactured V1.1 PCB target.

Phase 1 initializes only production-board controls, power status, the user
indicator, and I2C. GPIO1 LED output-enable remains LOW; no addressable LEDs
or buzzer tones are activated. The XIAO target provides serial diagnostics and
heartbeat output without initializing production PCB GPIOs. Production input
pull-ups are provided by the PCB, so front switches and rotary push are read
as active-low inputs.

Phase 2A adds a software-only 306-pixel framebuffer and the manufactured
channel/designator map. On XIAO startup, its self-test runs before a brief
status/map simulation. The production board does not illuminate LEDs during
normal startup.

## Phase 2B status

The production backend uses Adafruit NeoPixel 1.12.3 for the eight WS2812
channels. Production startup initializes channel data pins, sends an all-black
frame while GPIO1 `LED_ENABLE` is LOW, and leaves outputs disarmed.
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` prevents arming in the standard build.
The software current estimate uses a configurable 20 mA per full-scale RGB
component assumption and a 4000 mA LED firmware budget against the nominal 5 V,
5 A input path; the estimate is not a measurement of the board LEDs and excludes
MCU/peripheral load. Physical output remains disabled pending reviewed bring-up.

Build the bench target:

```powershell
pio run -e xiao-s3
```

Build the production target:

```powershell
pio run -e brooklyn-s3
```

## Licensing

- Firmware/software: Apache-2.0.
- Original hardware design: CERN-OHL-P-2.0.
- Original documentation: CC-BY-4.0.
- Third-party material retains its original rights and terms; see
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The full license texts are in `LICENSES/`.

The production scaffold deliberately leaves all 306 map/status LED outputs disabled. LED bring-up should be a separate reviewed step after the assembled board is electrically checked.
