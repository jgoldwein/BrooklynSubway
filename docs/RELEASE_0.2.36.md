# Brooklyn Map 0.2.36

- Add **Show last reported location** alongside **Hide** and **Show orange**.
  When movement is uncertain, a faint neutral white marker shows the last
  usable reported place and fades out over a configurable 1-300 seconds
  (default 60). White markers are capped at 2/255 and by the saved dim-light
  brightness and normal map brightness. Very dim fades occur in discrete steps.
- Repeated timestamps at the same uncertain location do not restart the fade.
  A newly reported location replaces the old one. Recent useful reports take
  priority; ambiguous conflicting reports and explicit removals clear markers.
- Keep temporary markers through missing reports or missing position fields.
  Stale data cannot refresh them. Markers are rendering hints, never added to
  occupancy/train counts. Route visibility, stale display shutdown and power
  caps still apply. No interpolation of geographic train movement is invented.
- Use bounded 256-trip history; if full, omit new hints rather than evicting
  active hints or restarting fades. Visit/fade history is not persisted across
  reboot. Hide/orange preferences saved by earlier firmware are retained;
  newly initialized settings default to last-reported mode with a 60-second fade.
- Save style, brightness and fade duration atomically in the existing settings
  record. Changing mode/duration does not reset history. Preserve prior API
  fields and add unknownTimingMode and lastReportedFadeSeconds.
- Add the simple live message "Some train movements are uncertain" when the
  latest poll contains uncertain station reports. Use plain-language controls
  and preserve the enlarged status-page buffer, OLED settings and beeps.

Validation covers repeated timestamps, missing polls/position, new stations,
conflicts, exact fade expiry, monotonic dimming, stale segments, route filtering,
zero/one brightness, known-stop priority, unchanged train counts, settings
persistence/legacy migration, generated form authorization, full production page
capacity, complete host regressions and all four supported build targets.
Physical visual validation remains pending.
