# Two additional line checks and monitoring improvements

Two complete checks of all 18 displayed route categories were captured on 2026-11-14 at 11:15:09 and 11:28:34 America/New_York. Both contain all eight MTA feeds plus the Subway Now route response, verified by recorded SHA-256 hashes. Website timestamps lag the capture reference by about nine seconds.

An intervening attempt timed out downloading the numbered-line feed. That attempt is retained as failed evidence and is not counted as the second complete check. During this work the PCB also reported 7/8 feeds, DegradedHold and a timeout on 1-7,S. A timeout at both clients does not determine whether its cause is upstream or local.

## Every line, both checks

Each item lists mapped-position observations / exact matching trip identities / usable website position estimates, as first check then second check. These observations precede fading and are not a count of physically lit LEDs. A matching next station does not establish GPS accuracy.

- 2: 2/2/2 then 4/4/4. Reference status in first check: 2: Delay.
- 3: 2/2/2 then 2/2/2. Reference status in first check: 3: Not Good.
- 4: 2/2/2 then 2/2/1. Reference status in first check: 4: Service Change.
- 5: 0/0/0 then 0/0/0. Reference status in first check: 5: Service Change.
- A: 7/7/7 then 7/7/7. Reference status in first check: A: Service Change.
- B: 0/0/0 then 0/0/0. Reference status in first check: B: Not Scheduled.
- C: 3/3/3 then 3/3/3. Reference status in first check: C: Service Change.
- D: 5/2/2 then 4/2/2. Reference status in first check: D: Service Change.
- F: 8/8/8 then 8/8/8. Reference status in first check: F: Delay.
- G: 9/9/9 then 10/9/9. Reference status in first check: G: Service Change.
- J: 5/4/4 then 6/6/6. Reference status in first check: J: Service Change.
- L: 12/7/5 then 12/9/6. Reference status in first check: L: Good Service.
- M: 2/1/1 then 2/2/2. Reference status in first check: M: Service Change.
- N: 5/3/3 then 6/4/4. Reference status in first check: N: Not Good.
- Q: 9/9/9 then 9/9/9. Reference status in first check: Q: Not Good.
- R: 5/5/5 then 5/4/4. Reference status in first check: R: Service Change.
- S: 5/5/4 then 1/1/0. Reference status in first check: FS: Good Service; H: Good Service.
- Z: 0/0/0 then 0/0/0. Reference status in first check: Z: Not Scheduled.

5, B and Z have no mapped-position samples in either check, so their live position agreement remains unassessed. B/Z are not scheduled in the reference snapshots; 5 reports a service change. S identities are present, but the second reference snapshot supplies no comparable shuttle position. All route categories were inspected twice; missing observations are not counted as passing location checks.

## Why roughly 80 trains can produce far fewer visible lights

The OLED previously reported the committed frame resolvedTrains count. Several trains can share one physical LED. Filters, black colors, brightness and uncertainty timers can make an occupied position dark. The inspected board uses Last reported, a 20-second fade, uncertain brightness ceiling 5/255, and global brightness 12/255. Last-reported markers also have the existing 2/255 cap, so they can be very faint and disappear after the fade. They do not persist forever merely because a fresh feed repeats an uncertain vehicle report. Older map frames can also be held during incomplete polls.

0.2.44 shows mapped trains and nonblack map colors sent by the live renderer separately: for example Trains:80 Lit:12. The lit count is taken after brightness and current limiting and excludes status LEDs. It cannot measure physical LED faults or human visibility at very low brightness. Diagnostic/startup lights are outside this live-frame count.

## Implemented monitoring

- Main status page and OLED show mapped and lit counts; the visible count comes from the live worker, so fades remain counted during blocking feed downloads.
- /monitor is a read-only, automatically refreshed page linked from Overview and Advanced. /api/monitor provides a bounded JSON download.
- Every line reports deduplicated mapped, moving, estimated-departure and uncertain counts from the latest poll, independent of route visibility filters. Moving/estimated/uncertain columns overlap.
- Per-line feed receipt distinguishes Received, Partial, Missing and Waiting. The S category depends on both ACE and G feeds because it combines H/FS. Receipt does not itself prove source freshness or valid decoding.
- Show committed-map update age, latest-poll age, oldest source-header age, current fading policy and the distinction between the committed map total and latest-poll total.
- Show old vehicle reports, unresolved/out-of-map positions, conflicting trips, conflicting prediction sets, unreadable feeds, capacity drops and the last recorded transport error.
- Count all eight expected feeds even when budget/preconditions prevent attempts, rather than implying fewer feeds are required.
- Keep the current colors, fading, brightness settings, power interlocks and train interpretation. No settings are changed by opening the monitor.

## Validation and scope

All 29 host suites and all four firmware targets pass. Targeted tests cover deduplication, skipped predictions, shared LEDs, faded/hidden/zero-brightness counts, status-light exclusion, per-cycle counters, source timestamps, partial shuttle feeds, JSON quoting, escaping and buffer limits. OLED runtime tests include the mapped/lit line. Browser checks cover 18 rows, automatic updates, delayed refresh and phone/desktop layouts. Native replay of all recorded cycles produces identical pipeline totals and mapped LED frames before and after adding monitoring.

The home page worst-case escaped fixture remains below its fixed 24 KiB capacity; the larger table lives on a separate page. The monitor uses existing buffers and adds bounded fixed-size counters rather than retaining raw feeds or trip logs. No new dependency.

Local evidence: captures/all-lines-two-more-044/ and captures/all-lines-two-more-044-retry/. Repeatable tools and earlier method: [train comparison](TRAIN_DATA_COMPARISON_2026-11-14.md). Public reference: [Subway Now routes](https://api.subwaynow.app/routes/?detailed=1).

Production build: 189,076 static RAM bytes (600 more than 0.2.43), 1,253,969 flash bytes. Runtime heap and physical visibility still need checking after installation.
