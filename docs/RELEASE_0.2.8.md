# Brooklyn Map 0.2.8

OTA pages now show an explicit completion message and automatically open the
status page displaying the running version. Completion requires the device
to report both the exact expected firmware version and successful SDK boot
health confirmation. A dropped connection, expired session, or full download
alone is not treated as installation success.

- Reconnect through public `/api/status` after restart/session expiry.
- Report `firmwareVersion` and `otaBootConfirmed` in public status JSON.
- Show **Update complete — Running version ...**, then open status after two
  seconds. Manual restart approval uses the same confirmation flow.
- Show download/verification, waiting for startup, failed installation, and
  unconfirmed completion distinctly. Stop automatic waiting after three
  minutes with a status-page link. HTTP requests have five-second timeouts.
- Preserve manual restart approval for staged images. OTA signature checks,
  activation policy, boot-health timing, rollback, and hardware behavior are
  unchanged. The health getter only reports a successful existing SDK call.

This behavior applies to pages served by 0.2.8. An older page already open
while installing this fix may still require manually opening the status page.

Validation: all 25 host runners and four firmware builds pass. Executed the
actual embedded page script against 15 behavioral assertions, covering
session expiry, expected version, health pending/confirmed, wrong version,
bounded waiting, timeout recovery, manual approval, automatic restart,
installation failure, and malformed/missing status fields. Worst-case
version-label test renders 3067 bytes into the 4096-byte controls buffer.
Production end-to-end completion observation remains pending.
