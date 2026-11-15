# Private COM6 build 0.2.64 - framed clock/weather and working weather connection

Changes requested by the operator:
- Thin frame around clock and weather, using existing OLED GPIO8/9.
- Screen order: Main, Activity, Connection, Device, Lines, Clock, Weather,
  Happy Bar Mitzvah. Stable enum IDs/MODE LED colors retained independently of
  browse position; serial page numbers use browse position.
- Clock ignores idle-return timeout, until a rotary turn/user navigation leaves it.
  POWER/setup/quiet rules still apply. Weather retains its ordinary timeout.
- Clock includes Outside temperature when cached weather is <=1h old. Unknown
  and old data are labeled rather than showing an invented/stale temperature.
- Weather screen has no source-credit row. Attribution remains in README.
- Correct spelling everywhere in active OLED/easter-egg text: Happy Bar Mitzvah!

Weather investigation: original3s handshake setting failed on COM6 with HTTP-1,
TLS-1,0bytes,~5272ms total. Diagnostic063 captured that failure and detailed heap
readings. In064 only weather gets8s handshake allowance, retaining2.5sTCP connect/
read idle and validated framework CA bundle; body deadline12s includes request
start. Normal MTA handshake/config unchanged. Failed weather retries at least
60s later in a new MTA-idle window; successful updates remain15min. No overlapping
MTA/weather/OTA TLS or new task/dependency/NVS setting. Serial logs now include
weatherHTTP/TLS/timing/heap diagnostics.

COM6 weather subsequently succeeded: HTTP200,TLS0,182bytes parsed,3370ms total.
This supports the timeout allowance change; do not claim the generic original
error was a certificate failure. Data is still Open-Meteo model data, not a sensor.
Actual OLED temperature/appearance remains an operator visual check.

All34 suites/four builds pass. Added exact browse-order/persistent-clock tests,
frames and outside-temperature render checks, one-minute failed retry boundaries,
retained malformed/stale parser coverage and all prior effects/control tests.
Static productionRAM198940B, flash1314241B, frozenapp1314608B.

Frozen064 flashed only verified COM6serial28:84:85:34:1F:74 at115200; all hashes
verified. OLED firstframe/healthconfirm and successful weather/MTA8-of-8 observed.
Initial boot had Wi-Fi-related partial cycle; later API10.1.1.27 verified expected
ID0000741f34858428,064,Fresh8/8. Physical LEDs were OFF/power-resume available at
that snapshot, so do not claim live output enabled. COM5 remains050 and untouched.
063 was an intermediate diagnostic image; do not publish it. Neither image has
been made a public stable release. Alarm clock and per-feed monitoring remain
future work; POWER/quiet-hours operator checks were previously deferred.
