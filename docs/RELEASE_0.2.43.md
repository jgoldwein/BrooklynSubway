# Brooklyn Map 0.2.43

- Recognize bounded, unambiguous skipped-stop departures from fresh trip predictions,
  including F express service. Show the next station in the selected approaching
  color when no physical segment exists, instead of retaining the old station.
- Retain the true reported departure endpoint, with no false departure indication
  at a skipped station. Preserve raw vehicle age and reject expired vehicles.
- Use bounded ETA timing to select existing segment pixels. Keep old/ambiguous/
  conflicting reports uncertain; reject stops explicitly marked SKIPPED or NO_DATA.
- Preserve configured colors, brightness caps, blink-free rendering and train counts.
- Add read-only, repeatable comparison tools and an all-lines audit report.

The recorded comparison finds additional valid departures on F and R. Live position
comparisons were available for 14 route categories, plus shuttle identity checks;
5, B and Z had no mapped samples in the capture. See TRAIN_DATA_COMPARISON_2026-11-14.md.

Validation includes synthetic skipped-stop and two-pixel bridge journeys, invalid/
stale/ambiguous/conflicting evidence, the original reported F case and three full
8-feed snapshots against 0.2.42. All host suites and four target builds pass.
On-board visual results remain to be checked after installation.
