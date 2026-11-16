# Automatic update rehearsal: 0.2.74 to 0.2.75

Passed on the COM6 production PCB on November 15, 2026. The operator enabled
Automatic updates, saved a five-minute check interval, and was instructed not
to press Check, Install or Restart. A read-only serial/API observer was started
before publication; no USB flash or manual web update was used.

Observed sequence:

1. Scheduled check found stable GitHub release 0.2.75.
2. Serial logged automatic installation, target brooklyn-s3, signed header
   accepted with key 1, then verified image staged.
3. Serial logged boot activation and automatic reboot.
4. API changed from 0.2.74 to 0.2.75 with a different boot ID.
5. Boot-health confirmation became true; the map resumed Fresh 8/8 feeds and
   live station output. The observer completed at 00:15:37 UTC November 16
   (7:15 PM New York time November 15), new uptime 20 seconds.
6. COM5 remained 0.2.50 before and after. Its automatic updates stayed off;
   its existing baseline test was not interrupted.

Overview now shows the running version and refreshes that value with live
status. The rendered 0.2.75 Overview field was checked on the actual board.

Evidence remains in local captures/auto-update-075/serial.txt, status.jsonl and
summary.json. These contain device diagnostics and are not public artifacts.
The observer required both automatic-install/activation serial markers, new
running version, boot-health confirmation and Fresh 8/8 before reporting pass.

This proves a successful scheduled unattended installation on this board and
network. It does not prove a forced physical rollback or a 12-hour stability
run. COM6 remains configured to check every five minutes with automatic updates
on; return to a longer interval after testing. The board must have working
power, Wi-Fi and internet to receive future releases.
