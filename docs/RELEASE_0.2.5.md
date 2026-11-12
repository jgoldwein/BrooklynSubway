# Brooklyn Map 0.2.5

- Add integrated administrator Hardware diagnostics at `/hardware-test` and a
  TEST-button hold (three seconds, then release).
- Fixed first test only: LED42, CH1 index 0, GPIO4, dim red 4/255 for two seconds.
  The existing interlock sends black first and last and leaves GPIO1 LOW.
  Normal map output stays compile-time locked; tests never run at startup.
- Require passed startup checks; web testing additionally requires a valid
  administrator session, CSRF token, and electrical confirmation checkbox.
- Set nominal supply to 5 V / 3 A and estimated LED budget to 2 A. This is a
  software estimate, not physical current protection.
- Exclude developer Wi-Fi/MQTT credentials from every versioned build and
  refuse signing a release binary containing local development credentials.
  Stored NVS Wi-Fi configuration is retained.

Validation: all 24 host test runners pass; all four PlatformIO targets build
with version 0.2.5 and pass the development-credential scan. Fake hardware
checks cover startup refusal, allocation failure, black/color/black sequence,
only one low-level red pixel, two-second dwell, enable LOW at completion,
and normal output API lockout. Physical LED observation remains pending.

Follow [the limited first-LED procedure](PRODUCTION_LED_DIAGNOSTIC.md).
Install with barrel power disconnected before preparing that manual test.

## Earlier release credential exposure

The 0.2.4 downloads were removed after a scan found embedded development
credentials in the existing fallback code. Earlier 0.2.0–0.2.3 releases also
matched those credentials and were hidden as drafts. Rotate the development
Wi-Fi and bench MQTT credentials from local `include/secrets.h`; deleting
downloads does not invalidate copies. No credential values are recorded here.
The OTA private signing key was not embedded or published.
