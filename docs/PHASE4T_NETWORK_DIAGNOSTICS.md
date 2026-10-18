# Phase 4T: bounded network diagnostics

## Scope

Phase 4T adds an explicitly user-triggered, read-only diagnostics run to the
existing station-mode LAN UI. It reports the current DHCP network identity,
valid system Unix time when available, DNS resolution for the configured NTP
hostname and MTA feed hostname, and a raw TCP connection attempt to the
resolved MTA address on port 443.

The TCP check is **not TLS or HTTPS validation**. It sends no HTTP request and
does not modify or replace the feed transport. The diagnostic does not change
Wi-Fi credentials, NVS, DNS configuration, SNTP policy, feed URLs, timeouts,
TLS settings, power limits, route/LED mappings, or physical-output state. It
does not clear configuration or start provisioning. UDP/123 reachability is
intentionally not probed.

## LAN interface

- `POST /network-diagnostics/run` requests one run and responds with HTTP 202.
  A concurrent/repeated request while one is queued or running returns HTTP
  409. The request takes no parameters and does not initiate configuration
  changes.
- `GET /api/network-diagnostics` returns the latest fixed-size snapshot,
  including Wi-Fi association, SSID, local address, subnet, gateway, primary
  and secondary DHCP DNS addresses, valid Unix time if available, and each
  probe state/address/elapsed time.
- The main page includes the last run state, per-probe states, existing
  transport counters and the latest retained feed failure summary. It states
  that raw TCP success does not imply TLS or HTTPS success.

The report excludes Wi-Fi passwords. HTML and JSON values are escaped by the
existing policy-layer writer; bounded output-buffer failure is returned as an
explicit HTTP 500 rather than a truncated success response.

## Execution and limits

The fixed-size runner proceeds sequentially: NTP DNS, MTA DNS, then raw TCP
port 443 when MTA DNS succeeded. It starts each probe at most once per run and
advances over separate firmware-loop iterations. DNS uses lwIP's asynchronous
`dns_gethostbyname` callback API instead of the blocking Arduino
`WiFi.hostByName()` wrapper. DNS is reported timed out after 4,000 ms; raw TCP
connect uses a 2,500 ms timeout. The lwIP callback context remains reserved
until its outstanding callback completes, so a later run reports that probe
busy instead of reusing a context still owned by lwIP. Callback-shared state
uses atomics.

When station Wi-Fi is disconnected, the runner completes with all network
probes marked skipped. A failed or timed-out MTA DNS check skips the dependent
TCP attempt. Successful raw TCP connectivity only means the socket connected.
SNTP synchronization status and feed transport counters are observed
separately; the diagnostic does not trigger feed polling or reconfigure SNTP.

## Host validation

`tools/run_network_diagnostics_tests.py` covers staged execution, bounded
timeouts passed to probes, disconnected behavior, failure/skip behavior,
reruns, output escaping, password exclusion, and small-buffer failure.
`tools/run_lan_web_policy_tests.py` covers LAN rendering and status API JSON,
including diagnostic states, transport counters, retained feed-failure
summary, and route preferences.

All 15 host regression runners passed: **1,554,092 checks, 0 failures**.
All four clean PlatformIO builds passed with
`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`:

| Environment | RAM | Flash | Delta vs. Phase 4S (RAM / flash) |
|---|---:|---:|---:|
| `brooklyn-s3` | 36,268 B | 304,689 B | +0 B / +152 B |
| `xiao-s3` | 35,380 B | 290,005 B | +0 B / +152 B |
| `brooklyn-s3-network` | 86,468 B | 1,076,985 B | +3,832 B / +6,660 B |
| `xiao-s3-network` | 85,580 B | 1,060,269 B | +3,832 B / +6,764 B |

Linked-ELF inspection found no physical initializer, arm, transmit,
NeoPixel `show()`, or Phase 4Q safety-controller symbols in either network
image. The XIAO image retains the logical `brooklyn::led::render()` symbol;
the production-network image does not retain it. The manufacturing board and
pin map are unchanged.

## Hardware checkpoint

This document records implementation scope, not a claim of completed hardware
validation. The XIAO ESP32-S3 is a bench/toolchain target only; no production
PCB upload is permitted. Exercise the real page and diagnostics GET/POST only
after verifying the XIAO target and its physical output remains disabled.
Record actual DHCP/DNS values, all three probe outcomes and elapsed times,
SNTP state, and feed-poll state. Do not infer XIAO results from host DNS or
network access.

The diagnostic-only `xiao-s3-network` image
(`D7382220CD20C363C526F1638426172C2F2A4F607C4A0FB7A6BEF74C3847E42B`) was
uploaded to the verified XIAO bench target on COM4. Esptool verified all flash
writes. No production target was uploaded.

On the live XIAO, `GET /` returned HTTP 200 (6,315 bytes) with the diagnostics
section; `GET /api/network-diagnostics` returned `not-run`; and
`POST /network-diagnostics/run` returned HTTP 202. Two separate runs completed
and were read back through the API. Both reported Wi-Fi associated at
`192.168.137.25/24`, gateway `192.168.137.1`, DHCP DNS primary and secondary
`0.0.0.0`, no valid Unix time, NTP DNS `failed` in 0 ms, MTA DNS `failed` in
0 ms, and raw TCP/443 `skipped` because MTA DNS did not resolve. Password
fields were absent. The separate status API continued to report
`timeSynchronized=false`, `feedCycleState=NOT RUN`, 0/0 feeds,
`physicalLedOutputEnabled=false`, brightness 64, and all 18 routes visible.
No credentials, NVS settings, transport policy, or host network configuration
were changed.

This is consistent with the previously observed XIAO DNS/feed limitation and
the lack of DHCP DNS servers reported by the diagnostics; it does not
establish an upstream TLS/HTTPS result or identify a firmware defect. The
diagnostics are exercised on hardware and expose the blocker without changing
network policy. The production PCB remains untested.
