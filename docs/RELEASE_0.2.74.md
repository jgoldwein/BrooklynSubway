# Brooklyn Subway 0.2.74

Consolidates the COM6-tested improvements since 0.2.50 for the shipping
candidate and internet-update rehearsal.

- Clearer phone Wi-Fi setup, OLED instructions, AP-ready beep and a 10-second
  reminder before opening the map on home Wi-Fi.
- Eight OLED screens: train activity, connection/device details including MAC
  and uptime, subway lines, a stable fixed-spacing clock, Brooklyn weather with
  Fahrenheit degree symbols, and Avi's dedication.
- Responsive rotary navigation and clicks; MODE temporarily focuses one line.
  Whole-line RGB tests light each selected line in its entirety.
- Configurable quiet hours for map lights, OLED brightness and sounds; startup
  greeting can remain until Wi-Fi connects. Quiet hours wait for a known clock.
- SYSTEM, NETWORK and MTA readiness indicators, with short green-transition
  cues, plus dim feed-ready U23. Station lights wait for usable train data.
- Advanced > Get help creates a privacy-filtered support report. Copy or save
  it, or open an email draft to Joel. Longer drafts use a summary and manually
  attached full report. Nothing is sent automatically.
- Plain-language quick start and user guide, plus optional hidden Avi effects.

Production target: ESP32-S3-WROOM-1U-N16R8. Production live output retains the
16/255 brightness cap and power budget; diagnostic tests remain dim. No wiring,
board-file, dependency, signing-key or MTA representation-policy change here.
The XIAO package is a bench/toolchain target, not production PCB wiring.

Validation: all 34 host suites pass, including OTA, safety, UI bounds and
hardware-isolation checks. Support-report browser checks pass at four widths,
including secret-field exclusion and copy/email fallbacks. Earlier private
builds were exercised on COM6; actual 0.2.74 GitHub download/restart/health
confirmation and the approximately 12-hour shipping-board run are separate
acceptance steps and must not be inferred from these build checks.

Select the package matching your hardware. Only signed packages are accepted.
Keep power connected during installation. This release is intended to preserve
Wi-Fi credentials and settings. Remote updates require a booting, connected
board; local Wi-Fi recovery and USB recovery remain available.
