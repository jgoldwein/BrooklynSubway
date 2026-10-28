# Phase 4X — Host-side virtual map and frame simulator

## Scope and safety

Phase 4X adds a local diagnostic simulator. It is host tooling, not firmware
and not a production display path. It neither uploads firmware nor contacts a
device. The host renderer is built from the logical semantic tables, frame
builder, RGB renderer, and Phase 4M operational display overlay; it does not
link or call `display_output`, `led_backend`, NeoPixel, GPIO, LED arming, or
strip transmission code. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` remains
unchanged.

## Authorities and geometry

- **Semantics:** the simulator's metadata exporter compiles and reads the same
  `brooklyn::semantic::kMapLeds`, `kStations`, `kRouteNames`, and physical
  chain table used by firmware:
  `src/generated/semantic_tables.cpp`,
  `include/generated/semantic_tables.h`, and `src/led_map.cpp`.
  Physical channel GPIO values come from `include/board_pins.h`.
- **Cross-check:** every generated LED is checked against
  `docs/mapping/phase3b/phase3b_semantic_mapping_worksheet.csv` for physical
  identity, route, role, station/endpoint identities and names, and placement.
  A mismatch or unresolved row fails export.
- **Geometry:** the complete tracked
  `docs/mapping/phase3b/led_positions_v11.csv` supplies exact V1.1 manufactured
  PCB X/Y/angle for all 306 addressable LED designators. The mapping input note
  identifies its source as the manufactured
  `Brooklyn_Active_Subway_Map_V1.1.eprj2` project (SHA-256
  `3ecb90c15d79f027a949c047f9c0a6e2f544706750498f9c7cb94209d61b4bc9`,
  document UUID `74298b6af49b4919b9aa371879fcf505`). The CAD project itself is
  not in the repository; the complete coordinate export is. The Phase 3B
  completion audit records exact agreement of all 306 placements with the
  worksheet and sub-tolerance agreement with accepted CPL coordinates.
- `analysis/projected_led_centers.csv` is an alternate accepted projection for
  302 map LEDs. The simulator uses the exact PCB placement coordinate system
  instead: it is the single complete, source-identified coordinate table for
  map and front-panel addressable status LEDs alike. No coordinates or
  semantic assignments are inferred. No background image is needed; the SVG
  renders these exact positions in a coordinate-space view and inverts Y only
  for SVG's screen coordinate convention.
- U23 is not in the chain, metadata, or frame. It remains a separate GPIO14
  indicator and is explicitly excluded.

## Architecture and data synchronization

`tools/virtual_map/host.cpp` is a host-only adapter. It emits static LED
metadata from the compiled firmware tables and accepts occupancy/configuration
input to run the real `TrainFrame` validation, `renderTrainFrame`, Phase 4M
`Controller`, and `applyStatusOverlay`. The adapter never calls the Phase 4N
output bridge. `tools/virtual_map/core.py` joins those compiled semantics to
the PCB coordinate export, validates the mapping worksheet, and deterministically
generates `tools/virtual_map/generated/metadata.json`. The JSON includes a
format/generator version and SHA-256 hashes of all semantic, geometry, and
generator inputs. Tests compare the committed JSON byte-for-byte with freshly
compiled firmware metadata.

The browser is a dependency-free local HTML/CSS/JavaScript page. Browser
JavaScript handles inspection and diagnostic presentation only: map colors,
route visibility decisions, brightness scaling, occupancy, and status colors
come from the compiled C++ renderer. Route selection sends the canonical
18-bit mask to that renderer; it does not reimplement route filtering or
colors.

## Launch and controls

From the repository root:

```powershell
python tools\virtual_map\run.py
```

The server binds only to `127.0.0.1` (default
`http://127.0.0.1:8765/`) and opens the browser. Press Ctrl+C to stop it.
`--no-browser` suppresses browser launch; `--port N` selects another local
port. `--export` regenerates the static metadata JSON, and `--check` checks it
without starting the server. The host C++ binary is built in a temporary
directory and removed when the server exits.

For a continuously updating view using the public MTA GTFS-Realtime feeds:

```powershell
python tools\virtual_map\run.py --live-mta
```

Live mode polls the eight compiled feed endpoints over HTTPS every 30 seconds
by default, using certificate verification and a 12-second per-feed timeout.
Requests run concurrently; each completed cycle is passed through one
persistent instance of the firmware host replay pipeline so operational display
state is retained between cycles. The browser shows per-feed results, update
time, current trains, and the rendered map. `--live-interval N` changes the
interval (15–3600 seconds); `--live-timeout N` changes the per-feed timeout
(greater than zero and at most 120 seconds). The browser/server remain
loopback-only. Live mode needs an internet connection, but not an ESP32-S3 or
MTA API key. Press Ctrl+C to stop polling and the local server.

This remains a host simulation: it displays trains resolved by the firmware
pipeline, not a camera view or a physical-device connection. All 306 addressable
LEDs use the exact manufactured PCB coordinates; the separate discrete U23
indicator is not drawn as an addressable point.

The map shows all 306 addressable points at their PCB X/Y positions, with RGB
as fill and channel-colored outlines as an optional diagnostic overlay. Off
points remain visible, focusable, and selectable. The inspector reports
designator, global index, channel/index/GPIO, type, route, station or segment
endpoints, coordinates, current RGB, occupancy/train metadata, and the
firmware-computed route-visibility state. The generated audit table contains
all 302 map LEDs.

Controls include:

- scenarios for off, station, segment, multiple routes, collision, hidden
  route, all routes hidden, reduced brightness, all six Phase 4M display
  states, and representative status LEDs;
- brightness 0–255 and the canonical route mask (default `0x0003FFFF`);
- status inputs for system health, Wi-Fi indicator, and provisioning mode;
- designator/station search, route/type/channel filters, occupied highlighting,
  and channel identification;
- JSON frame save/load.

Inspection filters only dim points/table rows; they do not modify frames. The
route-mask control is the sole visibility control that changes renderer input.
Status LEDs are separate from route filtering and map RGB.

## Map artwork underlay/overlay

The toolbar's **Map artwork** loader places a user-supplied PNG/JPEG/SVG/WebP in the same PCB coordinate space as the LEDs, under or over them. Opacity, scale, X/Y offset, and rotation sliders align it to the LED positions. The image is read in the browser only (stored in `localStorage`, never uploaded), and no artwork is bundled in the repo. LED fades, glow breathing, and an arrival flash are client-side visual effects between firmware-rendered frames; they do not alter the rendered colors.

The default underlay is the MTA Brooklyn subway diagram (`tools/virtual_map/artwork/brooklyn_subway_diagram.jpeg`, served as `/map-image.jpg`). It is registered to the LED coordinates with a least-squares affine transform fitted to nine landmark stations (constants `MAP_JPG` in `index.html`); the residual is about one LED diameter at worst. The four status LEDs have no place on the diagram and appear below it. The map also serves `/board-art.svg`, the top silkscreen and board outline rendered from the fabrication Gerber ZIP by `python tools/virtual_map/extract_gerber_art.py` (needs `pip install gerbonara`, host tooling only), toggled with **PCB silkscreen**. The Gerber's offset is fitted by matching all 306 LEDs to top paste pads. The Gerber is read-only input, and its proprietary "Colorful" layers are encrypted and not used.

### Manual alignment and approval

The **Fine alignment** bar nudges the artwork (arrows, scale, rotate, or **Drag map to move**) with fine/medium/coarse steps. **Edit 4 corners** shows a handle on each artwork corner; drag them, or focus one and use the arrow keys, to perspective-warp the diagram onto the LEDs. A move that would make the quad non-convex is ignored. **Reset corners** restores the original registration. **Approve alignment** writes the sliders and the four corners to `tools/virtual_map/artwork/alignment.json` (loopback-only, validated server-side), which is then the default; **Revert to approved** returns to it. The warp is rendered client-side to a canvas and only affects the diagram underlay, never firmware-rendered LED colors.

**All LEDs on** is a display-only override that lights all 306 LEDs magenta so they are visible against the diagram.

**Pin landmarks** aligns by example: tick *Pin mode*, then click the station on the diagram that matches each highlighted LED (suggested extreme stations first; *Choose LED* or an audit-table row picks another). With four or more pins the corners are re-solved by a least-squares homography and the status line shows the fit error in LED diameters. Use 6–8 well-spread pins, then **Approve alignment**. With 4–5 pins the corners are solved by a perspective fit; from 6 pins the map also *bends* (a thin-plate spline of the per-pin residuals) to follow local distortion between the MTA diagram and the schematic PCB layout. The status line shows the error before and after the bend in LED diameters; add pins where it is still off (8–20 total). **Reset corners** clears the bend. Approve saves the corners and the bend (`bend` in `alignment.json`, validated, max 64 points). Pins themselves are not persisted.

## Frame interchange

Frame JSON uses schema version 1. A saved frame contains exactly 306 logical
RGB triples (`r`, `g`, `b` in 0–255), `frameTimestamp`, optional
`trainFrame.occupancy` (306 values) and train metadata, operational display
state/status inputs, brightness, and route visibility mask. A saved RGB-only
frame can be opened and inspected offline without a renderer request. If
TrainFrame occupancy is present, the local host renderer can recompute RGB
after brightness or route-mask changes. No internet access is used.

## Validation

Phase 4X's focused runner is:

```powershell
python tools\run_virtual_map_tests.py
```

It verifies generated metadata synchronization and determinism, exact 306/302/4
and 276/26 counts, unique global IDs/designators, all channels/GPIO/indexes,
status indexes, U23 exclusion, all geometry, required Phase 3B semantic
regressions and Delancey route-specific station LEDs, route-mask behavior,
firmware brightness behavior, collisions, all Phase 4M states, map/status
separation, frame JSON shape, and invalid API input handling.

Validation:

- Phase 4X focused host suite: **80 checks, 0 failures**.
- All original host runners plus Phase 4X: **18 runners; 1,554,682 checks,
  0 failures**.
- Browser smoke test: local page and metadata loaded; one-station,
  multi-route, collision, route-hidden and DegradedHold scenarios rendered;
  306 SVG LED points, 302 audit rows, and four status indicators appeared.
- All four PlatformIO environments clean-built successfully. RAM and flash
  usage exactly match the Phase 4W baseline (zero-byte change): `brooklyn-s3`
  36,268 B / 305,097 B; `xiao-s3` 35,380 B / 290,413 B;
  `brooklyn-s3-network` 93,308 B / 1,116,665 B; and `xiao-s3-network`
  92,420 B / 1,100,089 B.
- All four environments retain `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`
  (network environments inherit the corresponding base-environment flags).
  Both network ELFs contain no simulator/`virtual_map`, NeoPixel transmit,
  LED-backend arm/write, or physical safety-controller symbols. The existing
  physical-output safety host suite passed **3,630 checks, 0 failures**.
- Python syntax checks and `git diff --check` passed. The generated metadata
  was re-exported and `--check` reports it current.

The simulator does not exercise live GTFS feeds or hardware/optical output.
Train scenarios are synthetic, deterministic `TrainFrame` inputs. The
production PCB remains outside this phase.
