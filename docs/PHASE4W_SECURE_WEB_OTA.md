# Phase 4W — Secure authenticated web OTA firmware update

Status: implemented, host/build verified, and hardware-validated on the
authorized XIAO ESP32-S3. The XIAO is running normal Phase 4W. The production
PCB is untouched; no production firmware was uploaded. Physical LED output
remains disabled (`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`).

## Scope

An administrator signs in on the normal LAN UI, opens `/update`, and manually
uploads a signed `.bkota` package for this hardware target. The device
verifies it and, only if everything passes, activates it and reboots.

Not included: automatic or unattended Internet updates, device-side GitHub
queries, eFuse, Secure Boot, Flash Encryption. The provisioning portal
(`BrooklynMap-Avi`, `192.168.4.1`) has no update route.

## Threat model and HTTP limitation

The LAN UI is plain HTTP. An on-path attacker can read or alter traffic, and a
hostile LAN client could attempt uploads. Therefore:

- Authentication (Phase 4V session cookie + CSRF) gates who may *start* an update.
- **The signature is what makes a package trustworthy**, not the transport.
  Unsigned, tampered, wrong-target or wrong-key packages are always rejected.
- A tampered upload fails verification; it cannot be installed.
- HTTP still exposes the administrator password/session to on-path observers
  (existing Phase 4V caveat). Use only a trusted/private LAN. The former
  Tailscale subnet-router plan is superseded and is not a current remote-access
  recommendation; the local web hop remains HTTP.
- Signed downgrades are allowed (the page and result warn). Anti-rollback
  version enforcement is not implemented.

## Partitions (no change)

All four environments already have two OTA slots, `otadata`, and NVS at `0x9000`.

| Env | Table | Slot (app0/app1) | Largest OTA payload |
|---|---|---|---|
| `xiao-s3`, `xiao-s3-network` | `default_8MB.csv` | 0x330000 (3264 KiB) | 3,342,336 B |
| `brooklyn-s3`, `brooklyn-s3-network` | `default_16MB.csv` | 0x640000 (6400 KiB) | 6,553,600 B |

NVS (`0x9000`, 20 KiB) is never written or erased by the OTA path; OTA writes
only the inactive app slot and `otadata`. Credentials, admin verifier, route
visibility, brightness and DNS preference survive. The on-device XIAO table
(read-only flash read) matched `default_8MB.csv`.

## Package format (`.bkota`, little-endian)

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | magic `BKMAPOTA` |
| 8 | 2 | format version = 1 |
| 10 | 2 | key id |
| 12 | 1 | target: 1 = Brooklyn production, 2 = XIAO |
| 13 | 1 | flags: must equal `0x01` (network-enabled build) |
| 14 | 2 | reserved = 0 |
| 16 | 4 | payload size |
| 20 | 32 | SHA-256 of the payload |
| 52 | 32 | version, ASCII, NUL padded, no spaces |
| 84 | 64 | ECDSA P-256 signature, raw `r‖s` |
| 148 | n | ESP32-S3 application image (starts with `0xE9`) |

Signed message = `"BROOKLYN-MAP-OTA-V1\0" ‖ the 84 header bytes`. Because the
header carries the payload digest, size, target, flags and version, the one
signature covers the binary and all metadata. The signature sits before the
payload, so it is verified **before any flash write**.

Target binding: a Brooklyn package is rejected by the XIAO and vice versa
(checked before the signature, and signed too). Flags other than `0x01` are
rejected, so images that allow physical LED output cannot be installed.

## Algorithm and library

ECDSA P-256 with SHA-256 through the mbedTLS that ships in the Arduino-ESP32
SDK (no new dependency; mbedTLS has no Ed25519). Ed25519 would have needed an
extra library. The public key table has two slots (current and next) in
`src/ota_trusted_keys.cpp`; an all-zero slot is untrusted.

## Keys and signing (offline)

Private key rules: never in the repo, Git history, release assets, docs,
firmware or NVS. `tools/sign_firmware.py` refuses private-key paths inside
the repository, and `.gitignore` excludes `*.pem`, `*.key`, `*.bkota`, `dist/`.

```
python tools\sign_firmware.py keygen --private-key D:\secure\bkmap_ota.pem --key-id 1
# paste ONLY the printed public entry into src\ota_trusted_keys.cpp, rebuild, flash via USB once
python tools\sign_firmware.py sign --firmware .pio\build\xiao-s3-network\firmware.bin ^
  --target xiao-s3 --version 4.23.0 --private-key D:\secure\bkmap_ota.pem ^
  --output xiao-s3-network-4.23.0.bkota
python tools\sign_firmware.py verify --package xiao-s3-network-4.23.0.bkota --private-key D:\secure\bkmap_ota.pem
```

Current key state: slot 1 (key id 1) is provisioned. The SHA-256 fingerprint
of its 65-byte uncompressed SEC1 public key is
`75cb7adec5d6d542cef403f65d9df5387816eb47a3bc237dcd27068754e93717`.
Slot 2 (key id 2) is empty and reserved for rotation.

The private key is stored outside the repository at
`C:\Users\jgold\.brooklynmap\ota\brooklyn_ota_p256.pem`. Joel must make and
verify a separate encrypted/offline backup. Losing this key means future
packages in this trust chain cannot be signed; USB recovery remains possible.
The key file itself is not stored in firmware, NVS, Git, or GitHub.

Safe rotation: release firmware signed by the current key that embeds the next
public key; install that firmware on devices; sign a later release with the
next key; retire the old current key only after devices have advanced. Do not
rotate keys as part of Phase 4W.

## Routes and authentication

- `GET /update` — admin page (session required; includes the CSRF token).
- `POST /update` — raw `application/octet-stream` body streamed through the
  WebServer raw callback. Requires, all checked before the first body byte is
  consumed: configured administrator, valid session cookie, and a valid
  `X-CSRF-Token` header. Multipart or other content types get `415`.
- Both are in the Phase 4V protected-endpoint policy. There is no
  Tailscale/LAN-address bypass.
- Status codes: 401 not signed in, 403 bad/missing CSRF or no admin, 413 too
  large, 422 invalid package, 503 no key provisioned, 500 flash error.

Success sequence: authorized → header parsed → target/flags/key checked →
signature verified → `esp_ota_begin` → stream + SHA-256 → digest matches →
`esp_ota_end` (image validation) → `esp_ota_set_boot_partition` → response →
reboot.

## Streaming, memory, failure behavior

No whole-image buffering in RAM or PSRAM; the 512 KiB GTFS buffer is unused.
Peak temporary buffering: the WebServer raw chunk (1,436 B, owned by the
library) plus the installer's 148 B header/signature scratch ≈ 1.6 KiB, plus
~300 B of heap objects per upload.

Any failure (auth, signature, digest, flash, truncated or aborted upload)
calls `esp_ota_abort`, never sets the boot partition, never reboots, and
leaves the running image and NVS untouched. An unconfirmed candidate is not
bootable.

## Rollback and health gate

The SDK has `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Phase 4W overrides
`verifyRollbackLater()` so a newly OTA-installed image stays "pending verify".
It is confirmed by `esp_ota_mark_app_valid_cancel_rollback()` only when:
setup completed with hardware-safe init and the pipeline self-check passing
(NVS configuration loaded, LED output disabled) **and** the loop has run
15 s. It does not depend on Wi-Fi or MTA data. If the new image crashes or
resets before confirmation, the bootloader reverts to the previous slot.
USB-flashed images are not pending-verify, so they are unaffected.

## GitHub Releases distribution

**Git tag ≠ GitHub Release ≠ signed OTA package.** A tag only names a commit;
creating one publishes no firmware. A GitHub Release is a separate object that
holds uploaded assets. A `.bkota` is a signed artifact built from the tagged
commit. For production all three are coordinated deliberately.

Per release, upload only: `brooklyn-s3-network-<version>.bkota`,
`xiao-s3-network-<version>.bkota`, `SHA256SUMS.txt`, and release notes.
The private key is never uploaded, and GitHub Actions has no signing key in
Phase 4W (build and sign locally).

Helper: `python tools\release_firmware.py --version 4.23.0 --private-key <pem>`
builds both network targets, signs, verifies, writes `dist\<version>\`
(`.bkota`, `SHA256SUMS.txt`, `RELEASE_NOTES.md`). It refuses a dirty tree and
a tag that is not at `HEAD`. Add `--create-draft` to create a **draft** release
with `gh` (requires `gh` installed and the tag pushed); otherwise upload the
files from `dist\<version>\` in the GitHub web UI. Publishing is always a
manual decision. The device `/update` page stays a manual upload; it does not
contact GitHub. A future device-side "check for update" is out of scope.

Release checklist: all host tests + clean builds pass → commit → tag →
push tag → `release_firmware.py` → review → draft release → publish.

## Verification results

- Crypto: P-256 ECDSA with SHA-256; package signature is exactly 64-byte raw
  `r || s` (32-byte big-endian integers). The signature covers the domain
  separator and all 84 header bytes. Mutating each header byte individually
  invalidated signature verification (84/84). The payload is SHA-256 hashed
  while streamed; a different payload was rejected before image finalization
  or boot-partition activation.
- Host tests: all 17 runners passed, **1,554,602 checks, 0 failures**.
  `tools/run_ota_tests.py` passed 207 checks, including 19 real ECDSA oracle
  verifications using ephemeral test keys.
- Four clean builds (RAM / image bytes / OTA slot use):
  `brooklyn-s3` 36,268 B / 305,097 B / 4.7%;
  `xiao-s3` 35,380 B / 290,413 B / 8.7%;
  `brooklyn-s3-network` 93,308 B / 1,116,665 B / 17.0%;
  `xiao-s3-network` 92,420 B / 1,100,089 B / 32.9%.
- `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` was confirmed in all four resolved
  PlatformIO environments. Both network ELFs have no physical strip transmit,
  NeoPixel `show()`, or physical-output/safety implementation symbols.
- XIAO identity was checked on COM4: ESP32-S3, 8 MB PSRAM, MAC
  `94:A9:90:D0:94:54`. Phase 4W was USB-flashed to the XIAO only; esptool
  verified each flash-data hash. The production PCB was not connected or
  programmed.
- Before and after USB installation, public status reported Wi-Fi connected,
  administrator configured, brightness 64, route mask 262143, manual DNS
  disabled, and physical output disabled. Credentials/settings remained
  present. No administrator password was recorded.
- The authenticated browser `/update` page displayed Phase 4W, target
  `xiao-s3`, the signed-package upload control, the HTTP/trusted-network
  warning, and the reboot/configuration warning. Anonymous GET and POST to
  `/update` returned 401. Authenticated POSTs with missing or invalid CSRF
  returned 403.
- On-device negative uploads were rejected: unsigned image (422), bad
  signature (422), unrelated P-256 signer (422), production target on XIAO
  (422), corrupt payload (422), incorrect digest (422), truncated package
  (422), malformed header (422), invalid flags (422), and oversized metadata
  (413). The XIAO stayed on the current image with configuration intact and
  uptime increasing.
- A real-key `Phase4W-OTA-Test-1` XIAO package (1,100,596 bytes) was host
  verified and accepted by the device. Serial reported the expected XIAO
  target, key id 1, header acceptance, streamed verification, and image
  activation. After reboot the distinct version was publicly reported, Wi-Fi
  and configuration persisted, and the old browser session returned 401.
- On the pending OTA image, setup health passed; after the 15-second gate,
  `esp_ota_mark_app_valid_cancel_rollback()` reported `ok`. The test image was
  running independently of Wi-Fi/DNS/MTA health.
- A separately signed XIAO-only `Phase4W-Rollback-Test` image used a temporary
  compile-gated hook to withhold confirmation and call
  `esp_ota_mark_app_invalid_rollback_and_reboot()` once after 20 seconds.
  Serial captured the request; the bootloader returned to the previously
  confirmed `Phase4W-OTA-Test-1` image. Configuration and physical-output
  safety remained intact. The test hook and PlatformIO test environment were
  removed before the final clean builds.
- A final signed normal Phase 4W package was installed through the browser.
  The device rebooted, reconnected, passed the 15-second health gate, and
  reported Phase 4W with administrator configured, brightness 64, route mask
  262143, manual DNS disabled, and physical output disabled. The stale session
  again returned 401; a fresh login and authenticated `/update` page worked.
- During the final OTA, serial milestones were observed and public status was
  sampled. Lowest sampled free internal heap was 237,592 B and free PSRAM
  remained 7,844,479 B. After reboot, free internal heap was 238,460 B and
  free PSRAM was 7,844,479 B. The implementation streams data; its scratch
  space is 148 B plus the WebServer raw chunk (about 1,436 B), not a
  whole-image allocation.
- In provisioning mode, a separate device joined `BrooklynMap-Avi` and
  `http://192.168.4.1/update` redirected to the setup page; no OTA UI was
  exposed. This portal has only the provisioning routes and a catch-all
  redirect.
- After the provisioning-portal check, the XIAO was reset and the OTA metadata
  was read without writing flash. `ota_seq=3` is valid and selects `app0` at
  `0x10000`; the prior `ota_seq=2` entry for `app1` is also valid. Serial
  heartbeats continued through 60 seconds with LED outputs disabled. A fresh
  public `brooklynmap.local/api/status` request returned HTTP 200 and reported
  Phase 4W, connected at `192.168.137.191`, administrator configured,
  brightness 64, manual DNS disabled, and physical output disabled.
- Production-board OTA remains unvalidated. Keep any first distribution as a
  draft/prerelease, explicitly state that limitation, and do not characterize
  it as production-validated.
