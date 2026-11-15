# Bounded local stability monitoring

This tool only reads device APIs. It does not install firmware, reboot the map,
change Wi-Fi/settings or run hardware tests. Requires firmware 0.2.49 or newer.
Keep this computer awake and connected to the board's LAN for complete coverage.

Example eight-hour run, explicitly pinned to the original board:

```powershell
& "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe" tools/monitor_stability.py --url http://10.1.1.56 --expected-device-id 00007cd036858428 --seconds 28800 --start-at 2026-11-15T22:00:00-05:00 --output-dir captures/overnight-2026-11-15
```

A rolling summary.json reports waiting/running/finished state; capture.jsonl
contains the evidence. The tool waits up to 30 minutes at the scheduled start for
the required firmware. It stops on a different board ID. Missing connectivity,
history overwrites, host sleep/coverage gaps and restarts are explicit, never
reported as successful samples. Runtime and status must agree on boot ID before
combining a health sample. Old failure/indication history at startup is baseline.

The eight-entry failure journal includes HTTP/TLS codes and heap/largest block
readings before attempts and before explicit close. A null TLS code means no
negative error was reported; DNS/TCP failure remains possible. The framework may
have cleaned up before that sample. Stack readings of zero mean not yet sampled
or that task is unavailable; nonzero readings are bytes on this ESP32-S3 port.

Restart context survives some warm resets in checksummed RTC memory. Power-on,
physical reset or corrupt retention may clear it. Counts are not lifetime boot
counts. The previous checkpoint is a main-loop snapshot, not a crash backtrace.
The external log retains observed boot changes across device power cycles.

Persistent lights can be legitimate stops or shared LEDs. They are review items,
not automatically errors, and active examples cover up to 12 positions. Completed
indication history is paginated with explicit gaps. Memory minima may include
activity earlier in the same boot; idle-window comparisons alone do not prove a
leak. Review the complete evidence before diagnosing TLS or memory failures.
