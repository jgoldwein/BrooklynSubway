# Brooklyn Map 0.2.27

- Stop treating every MTA STOPPED_AT report as a steady blue station light.
  A fresh same-platform arrival still ahead (up to 10 minutes) uses the existing
  orange-to-cyan approach indication. Departure-only predictions do not qualify.
- Steady blue requires two consistent stopped reports at the same mapped station
  from distinct newer feed timestamps, with the same nonzero movement timestamp,
  fresh prediction/feed evidence and a same-stop arrival within the past 30 seconds.
  Re-fetching the same feed is not additional evidence. This is corroboration,
  not independent proof of physical station dwell.
- All other uncertain stopped positions remain visible as a slow 2.4-second
  orange pulse. Old timestamps alone never invent departure or move the train.
  Without a new frame, blue station support expires after 30 seconds into the
  uncertainty indication. Genuine long holds remain visible as uncertain.
- Shared LED priority is corroborated stop > approach > uncertain, independent
  of contribution order. Occupancy and resolved train totals remain unchanged.
- Keep the existing verified-departure rule, route selections, brightness/current
  limits, POWER behavior, OLED and signed OTA safety. No GPIO/dependency changes.
- Why blue? now includes steady/estimated-approach/uncertain station LED totals.
  Its route table still describes raw STOPPED_AT candidates, not all-blue output.

Validation: station evidence transitions, repeated-feed rejection, shared-light
priority, pulse rendering and stale-support expiry, host regressions and all
production/bench builds. Captured-feed replay preserved 142/145 resolved trains
and 118/119 occupied LEDs, with no renderer rejections. Most previously blue
positions lacked corroboration and now pulse orange; uncertain is not moving.
Physical color and timing remain operator checks after installation.
