# Brooklyn Map 0.2.46

- Stop download delays from consuming a new stopped-color window. The window
  begins when the qualifying candidate is committed to the map. Repeated reports
  of the same station visit do not renew it, and held partial candidates do not
  start it. Movement or uncertainty can still end the indication sooner.
- Keep source and motion timestamps unchanged. A stopped indication also ends
  when its original vehicle, prediction or feed freshness limit expires; old
  evidence cannot gain a fresh display window.
- Move the bounded, sequential eight-feed download/decoder cycle to one dedicated
  worker. The main loop continues serving web pages, updating ages, servicing
  controls and aging the displayed map during downloads. Only completed results
  are handed back; the UI retains the previous complete diagnostic snapshot.
- Show "Fetching train data" separately from the last completed poll result.
  The existing 15-second setting remains the wait after a cycle completes.
- Defer release checks/installations, Wi-Fi setup, resolver changes and network
  diagnostics until a feed cycle has released its state. Authenticated manual
  package uploads during a cycle return a retry message instead of overlapping
  TLS/flash work. Authentication and signed-package verification are unchanged.

Validation: all 29 host suites and four PlatformIO environments passed. Added
publication-delay, held-frame, source-expiry, repeated-visit, clock-rollback,
ownership-handoff and fetching-state regressions, including 2,000 real host-thread
transfers. Browser checks passed at 320, 390, 768 and 1100 pixels, including live
fetching-state updates and preservation of unsaved settings. No GPIO, PCB, OLED-driver,
brightness, palette, dependency or per-line freshness-policy changes.

Resource cost: production static RAM 193468 bytes (4392 bytes above 0.2.45),
plus one 16 KiB worker stack and RTOS task bookkeeping allocated at first poll.
Allocation failure leaves the main loop responsive and retries after the normal
interval. Serial diagnostics announce the worker and its stack low-water mark.
Hardware responsiveness and available heap during simultaneous TLS/web activity
must be checked after installation; host tests do not validate the ESP32 driver.

Suggested check: watch a newly red station indication, refresh Overview/Train
monitor repeatedly while "Fetching train data" is shown, and confirm the OLED,
POWER and brightness controls still work normally. Red remains an evidence-based
indication with a maximum duration, not a promise of actual station dwell.
