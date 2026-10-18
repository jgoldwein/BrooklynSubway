# Phase 4V — LAN administrator access protection

## Status and scope

Phase 4V adds administrator authentication to normal-LAN configuration
operations. Read-only status remains public. The change does not alter the
manufactured PCB, GPIO mapping, physical LED-output interlock, Wi-Fi settings,
feed/TLS policy, route mapping, or brightness/power safety limits.

The first administrator password can be set or reset only from the existing
`BrooklynMap-Avi` provisioning portal at `192.168.4.1`. That portal is separate
from the normal-LAN HTTP server. Password changes made from the normal-LAN
interface require an active administrator session and CSRF validation.

## Normal-LAN routes

Public read-only routes:

- `GET /`
- `GET /api/status`
- `GET /api/network-diagnostics`

The status page and status JSON include only whether an administrator password
is configured and whether the current request has a valid session. They do not
include credentials, verifiers, session tokens, or CSRF values.

`GET /login` is public and issues a short-lived, one-use login CSRF challenge.
`POST /login` requires that challenge and is subject to login throttling.

Every normal-LAN operation below requires an authenticated session and a valid
session CSRF token:

- `POST /config/brightness`
- `POST /config/routes`
- `POST /config/dns`
- `POST /wifi/setup` and `POST /wifi/setup/confirm`
- `POST /network-diagnostics/run`
- `POST /reboot`
- `POST /admin/password`
- `POST /logout`

`GET /wifi/setup` also requires an administrator session because it presents
the protected Wi-Fi setup confirmation action. Unknown routes remain 404.

## Credentials, sessions, and request protections

- Passwords are 8–128 bytes. The verifier is derived using salted
  PBKDF2-HMAC-SHA256 with 10,000 iterations; the 16-byte salt and 32-byte
  verifier are stored together in the single `admin_v1` key in the existing
  `bkmap_cfg` NVS namespace. Credential operations touch no other NVS keys.
- Salt, session identifiers, CSRF tokens, and login challenges use the ESP32
  hardware RNG (`esp_fill_random`). Password verifier comparisons and
  confirmation comparisons use constant-time byte comparisons.
- There are two fixed server-side session slots. Sessions expire 30 minutes
  after login without sliding renewal; a new login replaces the oldest active
  slot. Logout removes only that session. Password changes invalidate all
  sessions.
- Login failures are counted globally on the device. Five failures trigger a
  30-second lockout. Login CSRF challenges are one-use, expire after five
  minutes, and use a two-slot fixed pool.
- Session and login cookies are `HttpOnly`, `SameSite=Strict`, path `/`, and
  time-limited. The `Secure` attribute is intentionally omitted because this
  server is plain HTTP.
- Password, confirmation, cookie, CSRF, and transient verifier buffers are
  cleared on the handled success and failure paths. No secret is printed or
  rendered into status output.

The provisioning portal accepts the password setup/reset form only while that
separate local provisioning server is active. Its Wi-Fi AP is open, and the
portal uses HTTP; users must keep the provisioning window physically
controlled and must not treat the AP as a protected network.

## Security limitations

The normal-LAN server is HTTP-only. Passwords, cookies, CSRF tokens, and page
contents can be observed or modified by a passive or active network attacker.
Authentication and CSRF defenses do not provide TLS, confidentiality, or
protection against an active man-in-the-middle. Use this interface only on a
trusted LAN. The current NVS configuration is not encrypted at rest unless
device flash/NVS encryption is separately provisioned. A reboot clears
in-memory login throttling and sessions.

## Validation

Host tests:

- `python tools/run_admin_auth_tests.py` — credential persistence failures,
  password validation, login, login-CSRF challenge use/expiry, session CSRF,
  logout, expiry, capacity/eviction, lockout, rollover, cookie parsing, and
  endpoint classification.
- `python tools/run_lan_web_policy_tests.py` — locked/unlocked HTML forms,
  CSRF inclusion, public status rendering, JSON fields, and secret-free output.
- `python tools/run_wifi_provisioning_tests.py` — local provisioning setup and
  reset form rendering; no password value is rendered.

The full host test suite (16 runners) and all four PlatformIO environments
passed. The Phase 4V network image was uploaded only to the authorized XIAO
ESP32-S3 bench target; the manufactured board was not flashed.

Live XIAO results (the user typed the password directly into the browser;
it was never seen or recorded):

- First-time administrator password set through the local `BrooklynMap-Avi`
  AP; `adminConfigured` became true and brightness, route mask and Wi-Fi
  were preserved.
- Locked state: public reads work; anonymous POSTs to brightness, routes, DNS,
  diagnostics, Wi-Fi change, reboot and password change return 401; LAN
  `/admin/setup` returns 404; the page shows the HTTP warning.
- A wrong password returns 401 with no session. Five wrong attempts give 429
  during the 30 s lockout, and 401 again after it.
- A correct login by the user unlocks the UI. Missing and invalid CSRF tokens
  changed nothing; a valid request changed brightness 64 to 65, then back.
- Logout locked the UI again. After an authenticated reboot the credential,
  settings and Wi-Fi persisted and the UI was locked until re-login.
- Re-entering the setup AP with `W` showed "administrator password is
  configured"; the device then returned to the LAN with settings intact.
- Public pages, status and diagnostics contained no credential, verifier,
  salt or token fields.

Deferred (covered by host tests only): two-session limit on hardware,
repeated login/logout cycles, submitting a password reset on the AP,
detailed heap tracking across the login cycle, and cookie replay of a
logged-out token.

Follow-up change in this phase: after a successful Wi-Fi save the setup page
shows only the result (LAN address and the `192.168.4.1` reminder), without
the Wi-Fi or administrator forms.
