# Brooklyn Map 0.2.6

- Give two brief flashes of the separate yellow Avi-location indicator U23,
  GPIO14/USER_LED, after local startup self-checks pass. Each flash is 150 ms,
  separated by 150 ms; the pin returns LOW. This occurs once per MCU startup
  (including reset/OTA restart), not whenever an additional supply is inserted.
- U23 works on logic power from USB or barrel. The addressable map and four
  status LEDs use the separate barrel-fed 5 V rail, so they cannot serve as a
  reliable USB-only startup indicator. No addressable startup sequence added.
- The startup flashes mean local firmware startup checks passed; Wi-Fi, time,
  and feed readiness are reported separately on the status page.
- Add LED42's location to the diagnostic page: F-line 18 Av, between Ditmas Av
  and Avenue I in the lower-middle map area. Its test remains unchanged.

Physical observation: user reported no visible LED42 light with barrel power
connected. Diagnostic serial result is not yet available. Do not treat the
software sequence completion as proof of physical LED operation; do not
increase brightness or enable additional map/status LEDs to work around it.

Tests cover U23-only pulses, correct timing, LOW at completion, no success
signal after failed startup checks, and no addressable LED_ENABLE activity.
All release builds exclude developer credentials and are scanned before signing.
