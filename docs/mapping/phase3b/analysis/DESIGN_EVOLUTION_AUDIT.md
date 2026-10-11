# Design-Evolution Provenance Audit

Review-only. The master semantic worksheet, station registry, firmware, and manufacturing sources were not edited. The accepted raster registration was not refit or changed.

## Scope and source coverage

- CSV covers **LED6–LED185 (180 rows)** plus **13 current VERIFIED rows outside that historical range**, for **193 total rows**.
- GTFS covers LED6–LED146: **141** designators, **141** with historical coordinates and current CPL placements.
- CP25 covers **39 designators LED147–LED185** with station/service assignments but no coordinates. CP19 supplies older geometry for **21** of them.

## Historical-to-current affine fit

Robust affine fit from GTFS EasyEDA placement (mm) to current CPL placement (mm), iteratively refined to a stable 1 mm inlier set: **115 inliers of 141 pairs**. This compares placement coordinates only and does not alter the accepted PCB-to-raster registration.

```text
current_x_mm = 0.9979994844 * historical_x_mm - 0.0001108148 * historical_y_mm + 0.8080440573
current_y_mm = -0.0008101324 * historical_x_mm + 0.9988163105 * historical_y_mm + 168.8403156977
```

- Residual <=0.5 mm: **111/141**; <=1.0 mm: **115/141**. Exact-match count at 0.001 mm: **0**.
- Median residual all points: **0.148 mm**; inlier median/RMS/max: **0.123/0.187/0.733 mm**.
- CP19 old-placement residuals: **1/21 <=1 mm; 2/21 <=2 mm; 19/21 >2 mm**. These indicate later placement changes, not semantic repurposing alone.
- Largest GTFS residuals: LED96 6.03 mm, LED7 5.44 mm, LED8 4.81 mm, LED6 4.43 mm, LED76 4.09 mm, LED136 2.36 mm, LED58 1.87 mm, LED50 1.83 mm, LED60 1.81 mm, LED61 1.74 mm, LED57 1.71 mm, LED52 1.69 mm.

## Current VERIFIED row audit

| Classification | Rows |
|---|---:|
| CONFIRMED | 13 |
| LIKELY_CONFIRMED | 31 |
| HISTORICAL_CONFLICT | 0 |
| CURRENT_VISUAL_CONFLICT | 6 |
| NEEDS_REVIEW | 1 |

Current-visual conflicts: **LED38, LED51, LED52, LED100, LED184, LED185**. Historical conflicts: **none**. Needs review: **LED113**.

## Full audit-table status totals

| Classification | Rows |
|---|---:|
| CONFIRMED | 33 |
| LIKELY_CONFIRMED | 33 |
| HISTORICAL_CONFLICT | 2 |
| CURRENT_VISUAL_CONFLICT | 6 |
| NEEDS_REVIEW | 119 |

**33** entries receive `SEMANTIC_CONTINUITY_HIGH` in the CSV when direct historical station/service, current high-confidence candidate, artwork, and <=1 mm coordinate continuity agree. CP25 itself lacks placement coordinates, so CP25-only rows cannot receive coordinate-continuity confidence.

## Design evolution and priority cases

- LED167 has direct route-history reassignment: CP19=Q, CP25=D. Current master remains ambiguous; CP25 is later design intent.
- **19/21** CP19 placements differ by >2 mm after transform. This establishes positional revision, not changed route semantics without separate evidence.
- No semantic promotion is made.

## Priority conclusions

- **Prospect Park:** V1.1 route lanes and CP25 expansion intent support **LED51=Q, LED184=B, LED185=S**. The current master instead says LED51 is Parkside Av Q, LED184 is Prospect Park Q, and LED185 is Prospect Park B; these are current-visual conflicts.
- **Parkside Av:** Historical GTFS identifies **LED52=Q at Parkside Av**. The V1.1 package is on the Parkside Q station marker; the current master calls it an interstation segment, a current-visual conflict.
- **Coney Island-Stillwell Av:** The artwork supports **LED165=F, LED166=N, LED167=D, LED7=Q**, matching CP25. LED165 and LED7 are VERIFIED; LED166/LED167 remain unverified in the master. CP19's earlier LED167=Q conflicts with CP25's D assignment; do not carry forward CP19 automatically.
- **W 8 St-NY Aquarium:** LED175=Q agrees with CP25. LED112=F agrees with the GTFS shared F/Q station record; CP25 adds route-specific Q LED175.
- **7 Av / 15 St-Prospect Park:** Original V1.1 artwork and GTFS placement continuity show the current master assignments are reversed: LED38 is at 15 St-Prospect Park F; LED100 is at 7 Av F.

## Constraints

The master worksheet and station registry were hash-checked before and after and remain unchanged. No firmware/manufacturing files, build, upload, commit, or push.
