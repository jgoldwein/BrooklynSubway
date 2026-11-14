# Brooklyn Map 0.2.38

- Add four saved color pickers under Train lights / Train colors. Defaults are
  approaching yellow (#FFFF00), stopped red (#FF0000), leaving green (#00FF00),
  and uncertain blue (#0000FF). Save all four together; Use default colors fills
  the pickers without saving until the user presses Save colors.
- Use one selected color per state. Leaving retains its bounded fade-out;
  uncertainty and last-reported markers use the selected uncertain color with
  existing dimming/fade behavior. Last-reported markers remain capped at 2/255.
  Mapped moving segment LEDs use the approaching color in live mode. Traffic
  remains blink-free; static route rendering/status/diagnostic colors are separate.
- Apply saved colors to the live worker without reboot or restarting visit/fade
  timers. Store one 12-byte RGB palette in NVS; absent/wrong-sized storage uses
  the requested defaults. Validate strict #RRGGBB input before saving.
- Update the Overview legend from the saved palette, including background status
  refresh without overwriting unsaved picker values. Replace old blue/white/orange
  labels with state-based wording and protect the new POST route with auth/CSRF.
- Preserve existing brightness/current caps and train counts. Black can hide a
  state's light intentionally. OLED title/time behavior is unchanged in this release.

- POWER short presses cycle: both on, LEDs off / OLED on, LEDs on / OLED off,
  both off, then both on. U23 follows the LED selection. OLED uses display
  power-save commands, retaining its configured address and brightness.
- Hold POWER for 3 seconds to request a reboot. Short action occurs only on
  release; a long hold does not advance modes or repeat. A button held through
  startup must first be released. Reboot waits for active loop operations to end.

Validation: all 29 host suites and four firmware targets pass. Browser checks at
320/390/768/1100 pixels cover fourteen forms, color picker/reset behavior, unsaved
values surviving refresh, navigation/save return, and complete status-page capacity.
The production build uses 188,428 static RAM bytes (96 more than 0.2.37) and
1,239,165 flash bytes (18.9% of its application slot). Physical color appearance
and runtime heap after installation remain to be checked on the board.
