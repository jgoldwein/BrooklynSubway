# Brooklyn Map 0.2.16

- Add an authenticated Enable / disable live display page. Production network
  builds support explicit live output; every reboot starts OFF. Confirm the
  verified 5 V, 3 A center-positive supply and electrical checks before enabling.
- Cap physical brightness at 16/255 (about 6%) and retain the existing estimated
  current budget. Logical brightness and route selections still apply.
- Run live rendering in a separate FreeRTOS output task using committed-frame
  snapshots, so blink timing and raw POWER shutoff do not wait for feed polling.
- POWER, Wi-Fi recovery, diagnostics, LAN shutdown, firmware installation,
  render/driver errors and hard-stale data stop the session. Black-first arming
  and black-before-disable cleanup are retained; enable again explicitly.
- Report live enable state on the status page, JSON and serial heartbeat.

No board or pin changes; no new dependency. XIAO and non-network production
builds cannot enable live output. The generic output APIs remain locked.
Physical brightness, colours, blink timing, POWER response and current require
observation on the production PCB after installation.

Validation: all host runners, dedicated live-policy and live-backend tests,
and versioned network builds pass. Existing channel and line diagnostics are
preserved; the complete-line ordering improvement remains queued.

Use: Status -> sign in -> Enable / disable live display -> confirm supply ->
Enable live display. Press POWER to stop.
