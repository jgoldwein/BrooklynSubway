# Brooklyn Map 0.2.34

- Add **Unknown train timing** settings: show/hide orange lights and set their
  brightness limit from 0-255. Try 1 for faint indications; zero makes them dark.
  Default remains shown with no extra dimming, preserving the previous display.
- Cover missing/stalled arrival timing, uncertain station reports and expired
  blue holds. Known approaches, unexpired credible stops and departure trails
  keep their existing behavior. Hiding an indication never removes feed data
  or changes the train count.
- Save both settings together across reboot and apply without restarting the
  device or resetting visit timers. Normal map brightness and existing physical
  current/brightness caps always take priority.
- Protect the new POST route with administrator authentication and CSRF checks.
  Include public read-only API fields unknownTimingEnabled and
  unknownTimingBrightness. Use plain-language labels and explanations.
- Preserve color-only motion, OLED settings, beeps, buttons and wiring.

Validation covers faint/zero/off rendering, preserved known arrivals/stops,
stalled predictions and expired blue holds, save/reload/failure/corrupt settings,
generated-form authorization, full host regressions and all four build targets.
