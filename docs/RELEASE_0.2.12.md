# Brooklyn Map 0.2.12

- Extend guarded Hardware diagnostics to CH2–CH8. Select exactly one channel
  per test; no automatic combined sweep. Counts remain 37/38/37/37/42/37/38/40.
- One red=4/255 pixel at a time, 500 ms each, approximately 19–21 seconds.
  Preserve black-first/last, POWER abort, startup checks, admin/CSRF/electrical
  confirmation, OTA exclusion, and final GPIO1 LOW. Other channels stay black.
- CH8 tests SYSTEM, NETWORK, MTA FEED, MODE first, followed by 36 map LEDs.
  Preflight validates channel bounds/count, first designator, physical roles,
  and canonical status mappings. Serial logs channel, GPIO, index/designator,
  and the first four status labels.
- Strict selection parsing permits only `first` or `ch1`–`ch8`; malformed
  values and embedded NULs are rejected. Diagnostic results link back to the
  test page. The original TEST-button LED42-only behavior is unchanged.

Normal physical live-map output stays compile-time locked. User already
confirmed the live 8-feed data path and CH1 tests; physical CH2–CH8 results
and controlled-load/current validation remain pending. Startup chime unchanged.

Validation: all 25 host runners and four firmware targets pass. Expanded
fake-hardware tests verify every channel's independent pin/count/pixel order,
CH8 status designators, complete and early/mid/late-aborted walks, black
cleanup, invalid channel/selection rejection, and normal API output lockout.
