# Codex prompt — Phase 3A.1 U23 Avi-location semantic correction

Apply this verified semantic correction only. Do not start Phase 3B route/station assignment yet.

## Authoritative correction

Avi's location indicator is the separate discrete LED **U23** on the `USER_LED` net, controlled by **GPIO14**. The manufactured schematic shows `USER_LED -> R60 1kΩ -> U23 (SZY0402Y) -> GND`, so GPIO14 HIGH turns it on.

U23 is **not** one of the 306 addressable RGB LEDs and is **not** part of any of the eight WS2812-style chains.

The addressable LED **LED291 remains an ordinary map LED**. Do not reclassify LED291.

Correct accounting remains:
- 306 addressable RGB LEDs total
- 4 CH8 status LEDs
- 302 addressable subway-map LEDs
- plus U23 as a separate discrete Avi-location indicator

## Required project updates

1. Update semantic naming/documentation so GPIO14 is identified as the Avi-location LED, designator U23.
2. Preserve the electrical facts: GPIO14, active HIGH, discrete output. Do not change the GPIO number or initialization behavior.
3. If the code currently uses a generic name such as `USER_INDICATOR_LED`, migrate to a clear name such as `AVI_LOCATION_LED` if this can be done without behavior change. A compatibility alias is acceptable if useful.
4. Ensure U23 is kept outside the 306-addressable-LED `led_map` / framebuffer topology.
5. Ensure Phase 3A logical accounting remains **302** addressable map elements, not 301.
6. Do not reclassify or exclude LED291 from route/station/segment mapping.
7. Add/maintain a special-indicator definition or documentation entry for U23.
8. Update `PROJECT_CONTEXT.md`, `AGENTS.md`, README/documentation, and the manufactured-board audit only as needed to record this newly verified semantic identity. This explicit prompt authorizes that narrow correction; do not otherwise rewrite protected reference material.
9. Do not modify `docs/MANUFACTURING_SOURCE_SHA256.txt` or any EasyEDA/manufacturing source.
10. Preserve all Phase 2B safety safeguards: `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`, GPIO1 LOW, physical addressable outputs disarmed.
11. Build both `xiao-s3` and `brooklyn-s3` and fix warnings/errors.
12. Do not upload, commit, or push.

## Report

Report every file modified, confirm U23/GPIO14 semantics, confirm LED291 remains a normal addressable map LED, confirm logical map count remains 302, and report both build results.
