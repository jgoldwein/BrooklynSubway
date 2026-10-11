# Phase 3B 302-LED projection and CPL cross-check

Projection-only deliverable. **No semantic classification was performed.**
The existing semantic worksheet, station registry, firmware, and manufacturing
sources were used as read-only inputs and were not modified.

## Primary image and accepted affine

- Primary raster: `C:\Users\jgold\OneDrive\Desktop\Brooklyn Subway Project\2D_Brooklyn MTA Active Map PCB_2026-10-10.png`
- Raster dimensions: 4096 × 3578 pixels
- SHA-256: `c5128ffa9e4387efd2ddb900596de2e810586bc16c16eccb40df0e7ff3fb2fb0`
- Coordinate input: `docs/mapping/phase3b/led_positions_v11.csv` x/y columns
  (EasyEDA internal placement units; not millimetres).
- Affine was kept exactly as accepted from the five supplied package-center
  measurements. It was not refit from borders, outlines, image extents, or routes.

```text
pixel_x = 0.28507458091615567 * pcb_x +2.3546071631019849e-05 * pcb_y -35.529927226501108
pixel_y = 5.6784348099641133e-06 * pcb_x -0.28508173461517872 * pcb_y +1191.9548886907905
```

The diagonal terms give approximately 0.285075 px per placement unit.
The off-diagonal terms encode the small fitted skew/rotation; they were preserved.
The negative image-Y scale accounts for raster rows increasing downward.

### Supplied anchor reproduction

| LED | Measured pixel center | Predicted pixel center | Radial residual |
|---|---:|---:|---:|
| LED42 | (1496.605, 2574.629) | (1496.631747, 2574.631823) | 0.026895 px |
| LED208 | (2289.111, 2861.155) | (2289.115418, 2861.154752) | 0.004425 px |
| LED165 | (1832.971, 3293.046) | (1832.960416, 3293.044495) | 0.010690 px |
| LED189 | (886.607, 1552.603) | (886.656556, 1552.601653) | 0.049575 px |
| LED163 | (1064.881, 1762.140) | (1064.810863, 1762.140277) | 0.070138 px |

## Independent CPL validation

- CPL workbook: `C:\Users\jgold\OneDrive\Desktop\Brooklyn Subway Project\PickAndPlace_Brooklyn MTA Active Map PCB_2026_10_09.xlsx`
- Addressable LED designators in CPL: 306
- Designators present one-to-one in CPL and placement coordinates: True
- Physical inventory split: 302 map LEDs + 4 status LEDs.
- Units/origin: CPL Mid X/Mid Y are mm; led_positions_v11 x/y are EasyEDA internal units consistent with mils.
- CPL-to-placement conversion: `placement_x_internal = CPL_Mid_X_mm * 39.37007874015748 + 600; placement_y_internal = CPL_Mid_Y_mm * 39.37007874015748 - 7980`
- Axis inversion between CPL and placement coordinates: none
- CPL side/layer counts: `{"T": 306}`
- CPL rotations (degrees/count): `{"0": 12, "133": 3, "135": 12, "180": 47, "223": 1, "225": 82, "226": 14, "227": 4, "250": 1, "255": 2, "270": 70, "312": 1, "313": 17, "315": 29, "45": 6, "90": 5}`
- Tolerance matches at ≤ 0.05 mm: 306/306
- Median residual: 0.000000000 mm
- Maximum residual: 0.000631505 mm
- Mismatches: 0

The CPL is an independent manufacturing cross-check; the authoritative
placement coordinates were retained for projection. The four status
inventory items are not included in the 302-row projected map CSV.

## Projected map samples

Samples cover each populated cell of a 3×3 grid over the map-LED extent.
A separate lower-right map-area sample is included from the Brighton Beach
corridor; the bottom-right rectangle of the full raster has no map LEDs.
These are projection checks, not semantic assertions.

| Image region | Designator | PCB coordinates (internal units) | Projected center (px) |
|---|---|---:|---:|
| upper-left | LED9 | (3035.0000, 1920.0000) | (829.717, 644.615) |
| upper-left | LED285 | (675.0000, 1115.0000) | (156.922, 874.093) |
| upper-center | LED89 | (7225.0000, 2095.0000) | (2024.183, 594.750) |
| upper-center | LED91 | (9085.0000, 365.0000) | (2554.381, 1087.952) |
| upper-right | LED268 | (11490.0000, 1800.0000) | (3240.019, 678.873) |
| upper-right | LED276 | (13545.0000, 365.0000) | (3825.814, 1087.977) |
| center-left | LED217 | (2565.0000, -1880.0000) | (695.642, 1727.923) |
| center-left | LED303 | (4960.0000, -2495.0000) | (1378.381, 1903.262) |
| center-center | LED196 | (7235.0000, -1845.0000) | (2026.941, 1717.972) |
| center-center | LED202 | (9095.0000, -115.0000) | (2557.221, 1224.791) |
| center-right | LED283 | (12120.0000, -1515.0000) | (3419.538, 1623.923) |
| center-right | LED92 | (9605.0000, -120.0000) | (2702.609, 1226.219) |
| lower-left | LED23 | (3125.0000, -5680.0000) | (855.194, 2811.237) |
| lower-left | LED227 | (4360.0000, -3750.0000) | (1207.307, 2261.036) |
| lower-center | LED61 | (7360.0000, -5270.0000) | (2062.495, 2694.377) |
| lower-center | LED229 | (5030.0000, -4370.0000) | (1398.292, 2437.791) |
| lower-right | LED115 | (8075.0000, -5940.0000) | (2266.307, 2885.386) |

## Visual registration gate

**GLOBAL REGISTRATION: PASS**

Inspected in `projected_led_centers.png`: DeKalb Av, Atlantic Av-Barclays Ctr,
Prospect Park / 7 Av, Brighton Beach, Coney Island, Culver corridor, eastern
Brooklyn, and northern/eastern route corridors. No systematic displacement
was visible; projected centers coincide with rendered package centers in the
inspected regions.

This is a visual registration result, separate from semantic classification.

## Generated files

- `projected_led_centers.csv` — 302 physical map LED projections.
- `projected_led_centers.png` — unobstructed-label center markers on the original raster.
- `projected_led_designators.png` — labeled projection overlay.
- `cpl_validation.csv` — all 306 addressable LED CPL-to-placement residuals.
- `projection_report.md` — this report.

All outputs are in `C:\Users\jgold\Documents\PlatformIO\Projects\BrooklynActiveSubwayFirmware\docs\mapping\phase3b\analysis`. No semantic proposals were generated.
