# Private COM6 build 0.2.54 - dedication and explicit uptime

- Device screen now explicitly labels Uptime and shows days / hours / minutes /
  seconds. Snapshot uses ESP32's 64-bit monotonic esp_timer_get_time rather than
  deriving total uptime from the wrapping 32-bit millis counter. Wi-Fi/time sync
  does not reset this value. Text remains bounded to 21 characters.
- Add a sixth selectable screen: Happy Bar Mitzvah!, Avi Yao, From the Goldweins,
  September 26, 2026, with the names Joel, Marlene, Marc, Kim, Juliana, Levi,
  Robbie, Eric scrolling in that order. The full name list is not limited by the
  SSID's 32-byte bound. All names become visible before the default 60-second
  idle return. The existing return-to-clock setting applies; Stay is available.
- Main remains the startup screen. One backward turn reaches the dedication;
  forward navigation reaches it after Lines. Counters now show six screens.
  The dedication's MODE indicator is pink under the existing brightness/current
  caps. MODE button remains reserved, with no action. Existing responsive rotary,
  clicks, Lines press/turn context, ROUTE and recovery behavior are retained.

No dependencies or pin assignments changed: OLED U22 uses GPIO8/9; MODE LED307
is channel 8 index 3 / GPIO13 data. No map rendering, feed polling, sound, web
settings or power-button policy change. No new NVS setting. Production static
RAM remains 198244 bytes according to the four-target build report.

All 31 host suites and four targets pass. Tests cover exact dedication/date,
all eight complete names visible through scrolling, six-screen wrap and Lines
context, dedication font bounds, 64-bit uptime across millis rollover and very
large uptime text. The user confirmed 0.2.53's knob response is much improved.

Prepared frozen dist/0.2.54/factory. Initial COM6 flash at 460800 baud lost USB
at 33% of the app write; the first 115200 retry could not connect. User requested
another attempt after the BOOT/RESET recovery prompt. Re-identified COM6 serial
28:84:85:34:1F:74 and retried the full bundle at 115200: all hashes verified,
hard reset completed. Serial confirmed time sync, OTA health confirmation and
live LED output resumed. 0.2.54 is now installed; the earlier partial image was
fully rewritten. Logs: captures/flash-054-recovery.txt and startup-054-*COM6.txt.
Operator still needs to verify the dedication appearance/scroll and uptime text.

COM5 master was not flashed and remains 0.2.50. A read-only sample during
recovery observed DegradedHold 7/8; retain this in stability observations rather
than describe the master as continuously 8/8. Build remains private, with no
stable GitHub release/tag.
