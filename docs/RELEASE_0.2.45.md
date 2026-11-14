# Brooklyn Map 0.2.45

- Start the stopped-color window when a stop report first qualifies through
  recent matching arrival evidence, rather than when an uncertain STOPPED_AT
  report first appears. This fixes cases where a 20-second window had only
  one second left when the light first became red.
- Keep the same visit deadline through noisy or repeated reports. A new station
  starts a new eligibility window; missing polls do not renew an old one.
- Preserve confidence checks, configured colors, brightness, fade settings and
  immediate movement/uncertainty changes. The setting is an upper bound, not a
  minimum guaranteed red dwell. Download delays and held older frames can still
  shorten the visible indication.

Regression reproduced the failure before the fix and verifies late qualification,
red/uncertain color boundaries, repeated reports and a new station. No new memory,
wiring, dependencies or network behavior.

Validation: all 29 host test suites and all four PlatformIO environments passed.
