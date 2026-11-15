# Brooklyn Map 0.2.49

- Fix first-boot setup Wi-Fi on a board with no saved credentials. Initialize
  the enabled network state before queueing provisioning, so the network loop
  starts BrooklynMap-Avi instead of returning as if networking were disabled.
  The user confirmed setup Wi-Fi appeared on the second production PCB (COM6).
- Capture per-attempt HTTP/TLS return codes, internal free memory/largest block
  before opening and before explicit cleanup, and lifetime minimum heap. Reset
  the framework's retained TLS diagnostic value before each attempt so a DNS-only
  failure cannot report a previous attempt's TLS error. Missing TLS detail is
  reported as unavailable, not as proof of successful TLS.
- Retain all failures in each eight-feed cycle and the last eight failures in a
  bounded RAM journal. Add read-only /api/runtime-diagnostics with failure IDs,
  current heap/largest block and feed/main/live-task stack low-water readings.
- Add stable board ID and per-boot ID. Checksum-validated RTC memory retains the
  previous main-loop uptime/memory checkpoint across warm resets when possible.
  Counters are scoped to that retained warm-restart session, not lifetime totals.
  Cold power/reset or corrupt retention can clear context; no crash backtrace or
  flash checkpoint writes are added.
- Add connection/restart details on Train monitor and an Advanced link. Add
  tools/monitor_stability.py for a bounded local overnight capture, with board
  identity checks, restart/gap detection, memory samples, source failures and
  persistent-indication review. Startup history is a baseline, not counted as
  new failures during the run.

Validation: all 30 host suites and four firmware targets pass; browser checks
pass at 320/390/768/1100 pixels. Host tests cover retained/cold/corrupt/saturated
boot context, journal rollover, missing TLS detail, full JSON bounds and monitoring
baseline/restart/gap behavior. A local HTTP integration check verifies bounded
capture and refusal of a different board. Hardware AP startup confirmed on COM6;
RTC crash-context retention still needs observation on an actual warm restart.
Production static RAM: 197812 bytes (+1216); RTC checkpoint: 36 bytes. No new
dependency, GPIO, display policy, power cap, TLS trust or retry-policy change.

Original board: update through the signed web updater. New unprovisioned board:
use the pinned local dist/0.2.49/factory USB bundle, on its verified COM port.
The separate captive-portal redesign follows this baseline release.
