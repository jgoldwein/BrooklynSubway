# Brooklyn Map 0.2.30

- Show an uncertain station report as steady dim amber, rather than blue.
  Blue now requires a matching TripUpdate arrival in the last 30 seconds,
  vehicle timestamp age at most 90 seconds, and prediction/feed age at most
  60 seconds. Missing, contradictory, future or old evidence stays uncertain.
  A departure-only event does not establish an arrival.
- Keep verified departure reconciliation and existing movement animations.
  Shared LEDs prioritize a recent matching stop, then an approach, then
  uncertainty, independent of contribution order. Train counts are unchanged.
- Add blue/amber LED totals to the station diagnostic. These describe the
  latest completed poll; a paused physical display can differ.
- Amber is fixed RGB 128/48/0 before configured brightness and the existing
  physical cap. It does not pulse, indicate measured motion, or increase power.
- OLED initialization/address fallback, startup/feed-ready beeps, buttons,
  GPIO assignments and power safeguards are unchanged.

Four public NQRW snapshots, 30 seconds apart on 2026-11-13, showed trips
changing stops while retaining STOPPED_AT. Replaying these through the updated
pipeline produced 16-20 blue and 1-3 uncertain amber map LEDs per snapshot;
verified departures and other movement indications remain separate. Many
recent matching reports still remain blue: this is a conservative interpretation
of feed evidence, not independent proof of station dwell.

Validation: all 29 host test runners pass, including station evidence,
collision priority, steady rendering, OLED, beeps, OTA and power safeguards.
Hardware visual comparison has not yet been performed for this revision.
