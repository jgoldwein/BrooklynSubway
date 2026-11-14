# Train-data comparison and skipped-stop reconciliation

## Findings

Compared three contemporaneous snapshots of all eight public MTA feeds with
Subway Now's actual web-map endpoint (`https://api.subwaynow.app/routes/?detailed=1`).
The snapshots were about 20 seconds apart; the website timestamp lagged the
capture reference by 9-19 seconds. Trip IDs were matched exactly, within the
same GTFS route, and physical coverage came from the firmware's decoder,
state builder and generated station/LED map. This measures interpretation,
not GPS accuracy or equality of every physical LED.

All 18 displayed route categories were inspected. The samples contained mapped
trains for 15 categories: 2, 3, 4, A, C, D, F, G, J, L, M, N, Q, R and S.
Subway Now supplied usable previous/next station positions for 14; Franklin
shuttle identities could be matched but no comparable web positions were
available. There were no mapped samples for 5, B or Z, so those live comparisons
remain unassessed. Static topology and adapter regression tests still cover
those routes. No-observation cases are not counted as successful comparisons.

The existing rule accepted only adjacent static stops. This missed southbound
F express journeys and some R journeys that omitted intermediate stops. In all
three snapshots, the candidate recognizes one additional F departure and two
additional R departures. Its actual streaming pipeline produces the same total
mapped train counts as 0.2.42: 85, 80 and 77. Estimated approach counts rise from
12/7/4 to 15/10/7 respectively. All feeds decode without errors, with no prediction
overflow or renderer rejection. Matching a target to Subway Now is not a claim
that its continuous estimated position has been independently verified.

Replayed the user's original F example separately at Unix 1794669190:
trip `053700_F..S`, reported `F24S` (7 Av), prediction `F27S` (Church Av),
vehicle timestamp 1794669042, feed/prediction timestamp 1794669182 and arrival
1794669241. Version 0.2.42 retains station LED100 at 7 Av. The candidate selects
Church Av LED40 as explicitly approaching, retaining 7 Av as the departure
endpoint. A synthetic regression covers the same skipped-stop/aged-report case.

## Firmware rule

- Require a newer prediction, a feed no older than 60 seconds, a prediction no
  older than 60 seconds, consistent direction and an arrival 1-600 seconds ahead.
- Verify one forward path by walking unique static predecessors, at most 12 edges.
  Ambiguous branches, unknown paths and reverse directions are rejected.
- Adjacent journeys retain the prior 90-second vehicle-age bound. Skipped-stop
  journeys may use the existing maximum vehicle age of 300 seconds. The original
  vehicle timestamp is preserved; fresh predictions do not keep expired vehicles alive.
- Retain the original detected station as the departure endpoint. Never substitute
  the local station immediately before the destination when an express train skipped it.
- Reject a first stop explicitly marked SKIPPED or NO_DATA and conflicting predictions.
- Use bounded elapsed-time/arrival-time progress only to select existing segment
  pixels. Where no segment is represented, show the next station in the configured
  approaching color. Never invent intermediate stops or count an extra train.
- Keep raw report age separate from fresh estimated-motion evidence. Departure trails
  still use the original report age, so a fresh prediction does not restart an old trail.

Remaining differences include prediction expiry, ambiguous service patterns, stale
vehicles, absent website positions, shared LEDs and independently timed snapshots.
These remain uncertain instead of being forced to agree with another estimate.

## Reproduce (read-only)

Use the existing host C++ compiler/host-test toolchain; the Python client uses only
its standard library. No new firmware dependency. For example:

```powershell
python tools/compare_subwaynow.py build --out captures/route-audit.exe
python tools/compare_subwaynow.py capture --out captures/route-comparison --cycles 3 --interval 20
python tools/compare_subwaynow.py compare --input captures/route-comparison --audit captures/route-audit.exe --output captures/route-comparison/report.json
```

Optional `--board http://10.1.1.56` adds public board status snapshots; it changes no
settings. `--doh` uses DNS-over-HTTPS for environments with broken system DNS;
`--ca-file PATH` supplies a trusted CA bundle while retaining TLS verification.
Capture metadata stores per-request timing, errors and SHA-256 hashes. The native
audit reports both merged per-trip interpretation and the actual streaming pipeline.
Duplicate-prediction safeguards remain enforced by the streaming pipeline.

Local evidence: `captures/all-lines-043-complete/` (ignored raw captures and JSON
reports), baseline commit `3cee2f1` / firmware 0.2.42. Capture tools never authenticate,
modify device settings or flash firmware.
