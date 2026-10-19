# Phase 4Y — Host-only GTFS-Realtime recording and replay

## Scope and safety

Phase 4Y adds host-side capture and deterministic offline replay of the public
MTA GTFS-Realtime feeds. Replay uses the firmware's compiled feed table,
streaming protobuf decoder, train pipeline, topology resolver, operational
display controller, and RGB renderer. The browser simulator presents the
resulting frames; it does not implement a second train-to-LED mapping.

This phase changes host tools and documentation only. It does not change
firmware sources, build configuration, physical LED output, OTA, or hardware.
It does not upload firmware, access a device, or contact hardware. The replay
bridge is compiled into a temporary host executable and is not linked into
embedded images.

Recording performs ordinary HTTPS GET requests to the eight URLs compiled
from `network::kFeeds` in `include/network_config.h`. TLS certificate
verification remains enabled. Each request has a bounded timeout and response
size. No API key, cookie, response header, or authentication material is
recorded. Fetch failures are retained as per-feed status metadata; only
successful response bodies are saved.

Real response bodies are third-party data. They are stored locally under the
Git-ignored `captures/` directory and are not committed or redistributed by
this project. Review applicable MTA/GTFS terms before publishing captured
payloads. Synthetic protobufs used by host tests are created in temporary
directories and are not live-feed fixtures.

## Capture

From the repository root, record ten cycles with a 30-second interval:

```powershell
python tools\replay\record.py --cycles 10 --interval 30 --timeout 12
```

By default, the recorder creates a unique session directory under `captures/`.
Each successful feed body is stored byte-for-byte in its own `.pb` file.
`manifest.json` records compiled feed order, cycle/fetch timing, fetch result,
byte length, and SHA-256. Payload and manifest writes are atomic. An interrupted
recording keeps fully completed cycles usable; an incomplete final cycle is
not added to the manifest.

The recorder intentionally does not retry failed requests or save response
headers. It makes one request to each endpoint per cycle, in firmware feed
order, then waits the configured interval before the next cycle.

## Offline replay and simulator

Replay compiles the same host bridge, checks the manifest against the current
firmware feed table, and verifies every payload's path, length, and SHA-256
before use. A second integrity check occurs when each payload is streamed to
the bridge. The original `.pb` files are never modified by replay or fault
injection.

Replay every cycle and export canonical JSON:

```powershell
python tools\replay\replay.py captures\<session> --all --export captures\<session>\replay.json
```

Open the replay in the virtual map simulator, initially paused:

```powershell
python tools\replay\replay.py captures\<session> --all --serve --step
```

The browser provides previous/next, play/pause, and playback speed controls.
Without `--step`, a served multi-cycle replay begins playing automatically;
`--step` opens it paused.
Selecting a cycle displays its recorded and simulated timing, feed completeness
and missing feeds, parser/pipeline counters, known observation targets,
collision locations and actual firmware RGB, and train inspection metadata
(trip/vehicle/train IDs, current/next stop, and previous-stop source). Route
visibility and brightness changes are sent to the compiled firmware renderer.
The simulator binds only to `127.0.0.1`.
Unresolved supported-route observations include route, current/next stop,
category/reason, and previous-stop evidence. These diagnostics identify data
for review and do not imply a semantic mapping should be changed.

Useful replay options include:

- `--cycle N` to replay through the one-based cycle `N`; `--all` replays the
  complete session.
- `--speed N` to scale recorded monotonic timing without changing protobuf
  payload bytes or timestamps.
- `--omit-feed FEED`, `--no-feeds`, and `--corrupt-feed FEED` to exercise
  missing-feed and parser-failure paths in memory only.
- `--delay-next-cycle-ms N` to test the existing stale-display timing policy.
- `--brightness N` and `--route-mask N` to select firmware renderer inputs.

The replay export uses schema `brooklyn-gtfsrt-replay`, version 1. Its
`captureManifestSha256` ties the output to the exact capture manifest, and its
`replayOptions` record the deterministic options used. Replaying the same
manifest with identical options produces byte-identical canonical JSON.
Simulator import rejects unsupported schema versions and malformed frame
shapes.

## Validation

Run the focused replay tests:

```powershell
python tools\run_replay_tests.py
```

The Phase 4Y focused suite passes **74 checks, 0 failures**; the Phase 4X
focused suite passes **80 checks, 0 failures**. All **19 host test runners**
pass **1,554,756 checks, 0 failures**. The tests use synthetic raw protobufs
to exercise the actual firmware
vehicle-only pipeline and verify deterministic replay, raw-byte/hash
preservation, feed-failure and corruption paths, trip-update metadata,
topology-derived previous stops, duplicate/conflicting trip handling,
collision metadata/RGB, route filtering, brightness, simulator export
validation, and the exact 90-second partial-grace and 300-second stale
boundaries. No network or hardware is used by the tests.

The full host suite is run with each `tools\run_*_tests.py` runner. The
Phase 4X generated metadata remains the authoritative 306-addressable-LED
geometry/semantic cross-check: 302 map LEDs (276 stations, 26 segments), four
status LEDs, zero unresolved rows, and U23 excluded.

The Phase 4Y host bridge is not an embedded firmware component. All four
PlatformIO environments were clean-built and matched the Phase 4X RAM/flash
baseline:

| Environment | RAM | Flash |
|---|---:|---:|
| `brooklyn-s3` | 36,268 B | 305,097 B |
| `xiao-s3` | 35,380 B | 290,413 B |
| `brooklyn-s3-network` | 93,308 B | 1,116,665 B |
| `xiao-s3-network` | 92,420 B | 1,100,089 B |

Both network ELFs were checked for the virtual-map/replay bridge, physical
backend initialization/arming/transmission, NeoPixel transfer, and physical
output safety-controller symbols; none were present.
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains configured. No device was
flashed, and the production PCB remains untouched.

## Current capture status

The local capture `captures/20261019T062411Z-464a7da9/` contains ten complete
cycles, all eight feeds succeeded in every cycle, and all 80 raw payloads
remain byte-for-byte hash-verified. Their combined size is 2,267,622 bytes.
The deterministic replay export has ten `Fresh` frames, 2,484 vehicles seen,
374 resolved trains, and zero parser failures. Across the capture the
renderer occupied 118 unique station LEDs, no segment LEDs, and reported 20
collision LED-cycles. Two independent full replays produced identical
canonical output with SHA-256
`b39513529049e353c67d8b6fc9d758231afbc6c0de11a34d2c1f09cc7e75c649`;
the comparison covers resolved vehicle results, TrainFrame occupancy and
metadata, operational states, and rendered RGB frames.

Replay surfaced 429 `UnknownStop` observations across 152 distinct route/stop
pairs. The inspection table and CLI summary now expose the exact observed
route, stop, category, and previous-stop evidence. These are firmware
pipeline classifications, not proof of a semantic-table defect; no route or
stop assignments were changed. The separately classified realtime-only
operational stop `H19N` appeared 100 times. The captured payloads and replay
export are local, Git-ignored third-party data; do not commit or publish them.
The sorted per-pair observation counts are preserved, without payload bytes,
in [PHASE4Y_UNKNOWN_STOP_COUNTS.csv](PHASE4Y_UNKNOWN_STOP_COUNTS.csv) for the
dedicated follow-up audit. They describe this capture only and are not a
mapping proposal.

This ten-cycle sample produced zero segment LED occupancies. This is not a
failure: segment resolution remains covered by the synthetic and host
regression tests. Future live captures should seek real segment examples;
this observation does not justify changing the resolver or segment mappings.

The browser smoke test loaded all 306 points, 302 map audit rows, four status
indicators, observation details, and actual-renderer collision RGB. First,
middle, and final cycle navigation, play/pause, search filtering, and York St
LED inspection passed. The `--step` launch starts paused; the default served
replay autoplay URL was also verified. The server bound only to
`127.0.0.1` and was stopped after testing.
