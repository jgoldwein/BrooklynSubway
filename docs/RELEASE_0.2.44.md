# Brooklyn Map 0.2.44

- Show mapped trains and lit live-map LEDs separately on the OLED and main page.
  Several trains can share one light; faded, hidden or zero-brightness positions
  are not counted as lit. Example OLED line: Trains:80 Lit:12.
- Add Train monitor links on Overview and Advanced, with a live per-line table,
  source/poll/map ages, feed receipt, uncertainty/estimate counts and JSON export.
- Report missing/unreadable feeds, rejected old reports, prediction conflicts,
  processing capacity problems and the current uncertainty fading policy.
- Keep all eight expected feeds in the denominator when some are skipped.
- Preserve train interpretation, colors, fading preferences and brightness caps.

Completed two further full eight-feed comparisons of all 18 displayed route
categories, repeating an attempt whose numbered-line feed timed out. 5/B/Z had
no mapped samples; reference shuttle positions were unavailable in one check.
See docs/TRAIN_MONITORING_RECHECK_2026-11-14.md in the repository for details.

All host suites, four target builds and desktop/mobile browser checks pass.
Lit counts are commanded colors, not a physical brightness or hardware sensor.
