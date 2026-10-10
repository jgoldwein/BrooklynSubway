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
status/map simulation; neither target initializes LED data GPIOs or transmits
LED data, and rendering on the production target remains a no-op.

Build the bench target:

```powershell
pio run -e xiao-s3
```

Build the production target:

```powershell
pio run -e brooklyn-s3
```

The production scaffold deliberately leaves all 306 map/status LED outputs disabled. LED bring-up should be a separate reviewed step after the assembled board is electrically checked.
