# Brooklyn Map 0.2.4

- Add Show passwords controls to Wi-Fi provisioning, administrator setup and password changes, and sign-in forms. Passwords start hidden.
- Fix fresh-board provisioning failing with "Unable to load administrator settings" when the configuration namespace does not exist yet.
- Physical LED output remains disabled on all targets.

Validation: all 22 host test runners and all four PlatformIO environments pass. OTA tests include 207 checks and 19 signature oracle verifications. Production USB-only operation before this release retrieved all eight feeds, synchronized time, and used external PSRAM successfully. Production OTA installation of this release still needs hardware validation.
