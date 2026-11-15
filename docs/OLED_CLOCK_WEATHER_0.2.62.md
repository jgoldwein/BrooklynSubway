# Private COM6 build 0.2.62 - time/date and Brooklyn weather

Eight OLED screens, preserving the existing first six IDs:
Main, Activity, Connection, Device, Lines, Dedication, Time & Date, Brooklyn weather.
Turn the rotary knob to browse. From Main, one backward step opens weather and
two opens Time & Date. The dedicated clock has only two centered lines: current
12-hour time with seconds/AM-PM and weekday/month/day/year. Both derive from one
New York/DST-adjusted timestamp to avoid inconsistent dates at midnight. Unknown
time is labeled rather than invented. Existing POWER, contrast/quiet policy and
idle-return setting apply; choose Stay on this screen to leave it displayed.
Startup dedication turn-to-skip explicitly returns to Main even with new pages.
MODE remains reserved; LED307 indicates clock white and weather blue, using
existing physical brightness caps. OLED GPIO8/9 and LED307/data13 unchanged.

Weather uses model-based current conditions from Open-Meteo at approximate central
Brooklyn40.6782,-73.9442, Fahrenheit, weather-code description and source-data age.
Updates roughly15min, only first second of idle time after an MTA cycle. Existing
feed worker runs either MTA OR weather; no new task or concurrent weather TLS.
Weather has separate2.5s connection/idle limits and6s body deadline, existing CA
bundle validation, a1024B view of shared PSRAM buffer, no NVS writes/API key/new
dependency. Worker result transfers through the existing release/acquire handoff.
Weather cannot modify MTA counters, pipeline, poll scheduler or processed mask.
Shared network-busy guard defers both automatic and manual OTA while weather runs;
weather is not mislabeled as fetching train data in the normal status API.

Open-Meteo supports CSV; HTTP/1.0 verified on host returns connection-close body
without Content-Length. Only weather accepts unknown length, capped1024B; parser
requires expected Fahrenheit headers, complete terminated row, bounded numbers,
plausible timestamp/temperature and no extra rows/NULs. Weather TLS closes before
main ownership resumes. Failed refresh retains prior data and labels delay;
source age>1h says old, >3h hides values/unavailable. Missing initial data says
waiting/unavailable with retry guidance. OLED includes Source: Open-Meteo;
README links documentation/CC BY4.0 attribution and free noncommercial usage.

All34 suites and four builds pass. Includes parser truncation/invalid units/null/
overflow, scheduling/rollover, stale/failed rendering, New York dates/buffers,
actual clock/date draw bounds/centering, eight-page rotary wrap, greeting skip,
MODE colors and background-weather OTA exclusion. Static production RAM198940B,
flash1313473B (~20% app slot), frozen app1313840B.

Frozen062 flashed only serial-verified COM6 28:84:85:34:1F:74 at115200; hashes
verified, OLED3C first frame, health confirmation, rotary navigation through
clock/weather, live frames and complete8/8 MTA cycle observed. Serial initial
capture also recorded OTA release-check invalid metadata length -1; this is a
separate release-discovery observation, not fixed by weather update. Operator
weather-temperature confirmation pending because initial weather download could
have occurred between serial capture windows. COM5 remains050, observerPID38652
running/waiting. No public release/tag/push. Alarm clock explicitly future/queued;
independent-feed monitoring improvements remain subsequent work.
