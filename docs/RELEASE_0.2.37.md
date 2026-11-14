# Brooklyn Map 0.2.37

- Redesign the main web page around five simple areas: **Overview**, **Train
  lights**, **Screen**, **Wi-Fi**, and **Advanced**. Use readable cards, larger
  controls, clear focus outlines, and layouts that work on phones and desktops.
- Put light power, data freshness, Wi-Fi status, mapped trains, feed health and
  a color guide on Overview. Keep technical details in closed, labeled panels.
- Group map brightness, uncertain-train behavior, blue hold and subway-line
  selection together. Use colored line badges and live slider value readouts.
  Screen brightness is separate from map brightness.
- Keep navigation visible while scrolling. Remember the current area for this
  browser session, including after a normal save/redirect. Show Saving... while
  submitting settings. Without JavaScript, anchor links, native details and
  normal forms remain usable.
- Give the sign-in page the same visual style and keep initial setup instructions
  visible when no administrator password exists. Use plain language suitable for
  a younger user; use **Advanced**, not More, for the technical area.
- Serve CSS and navigation JavaScript from the board at /ui.css and /ui.js,
  with versioned local URLs and bounded caching. No external libraries, fonts,
  analytics or content services. Keep the existing 24 KiB static page buffer.
- Preserve form names/actions, administrator/CSRF checks, background status
  refresh, all saved settings and the existing train/OLED/hardware behavior.

Validation: complete host regressions and all four firmware targets. Browser
checks at 320, 390, 768 and 1100 CSS pixels cover every area, expanded details,
signed-in/locked/setup/no-data states, all thirteen form field inventories,
line selection, keyboard focus, live slider readouts, unsaved input retention
during refresh, returning to the same area after save, and no-script fallback.
No horizontal page overflow or browser runtime errors were observed. The full
production/escaped page remains within 24 KiB. Live-board visual verification
of this revision remains pending until installed.
