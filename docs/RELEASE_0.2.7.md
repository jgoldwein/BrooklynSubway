# Brooklyn Map 0.2.7

- Add CH1 walk to the administrator Hardware diagnostics page: 37 physical
  pixels in order, one dim red pixel at a time (4/255), 500 ms each, then black
  and output disabled. About 19 seconds. Hold POWER to abort between pixels.
- Preserve the LED42-only two-second test and TEST-button behavior. All tests
  require passed startup checks; web tests also require admin session, CSRF,
  and electrical confirmation. Normal map output stays locked.
  Web tests are refused while an OTA download/upload is in progress.
- Fix the OTA progress page ignoring expired sign-in sessions after restart.
  It now returns to public status on HTTP 401/403, shows reconnecting text on
  errors/timeouts, and offers a running-version link. This improvement applies
  to pages loaded from the new firmware; an older open update page may still
  require manually opening status after this installation.

User observed the F-line 18 Av LED light red in the initial test. CH1 walk
physical validation remains pending. No physical test is triggered by install.
The U23 startup indicator remains unchanged. Hardware: existing GPIO4 CH1,
GPIO1 LED_ENABLE, and active-low GPIO40 POWER input. No PCB/pin-map changes.

Validation: all 25 host test runners and all four firmware targets pass.
Fake-hardware checks verify 37 ordered CH1 pixels, exactly one red=4 pixel at
a time, 500 ms dwell, black-first/last, POWER abort, final enable LOW, and
normal-output lockout. OTA page script passed eight mocked response/reconnect
scenarios. Release packages are credential-scanned and signed.
