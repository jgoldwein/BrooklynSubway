# Phase 4K: hardware network bring-up (XIAO ESP32-S3 only)

**Scope:** XIAO ESP32-S3 bench board only.

- **Production PCB schematic audit:** GO for USB-only bring-up with the
  external 5-V/barrel supply disconnected. The schematic shows USB VBUS ->
  U21 -> 5V_LOGIC -> U18 -> 3V3, external barrel input -> 5V_RAW -> U16 -> 5V
  LED rail, and the addressable LEDs on the separate 5V rail.
- **Production PCB physical validation:** NOT YET PERFORMED; the manufactured
  PCB is not available.

`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` throughout; no LED output, no
LED_ENABLE activity, no external LEDs. No credentials are recorded here. The
hardware runs used a temporary local `-DBROOKLYN_NETWORK_ENABLED=1` on
`xiao-s3`; it was reverted and is not repository policy.

## Conclusions (XIAO hardware validation)

- Wi-Fi connection, SNTP, and certificate-validated HTTPS work.
- MTA GTFS-RT feeds download and decode; the end-to-end TrainPipeline works on
  the physical ESP32-S3.
- Complete 8/8 cycles occurred. Partial cycles retain the successful feeds and
  no partial protobuf data was processed.
- Physical LED output remained disabled.
- **Wi-Fi recovery:** after about 6,500 s with the configured SSID
  unavailable (`WiFi.status()` = 1, `WL_NO_SSID_AVAIL`), the existing firmware
  reconnected automatically. No reboot, re-upload, credential change or manual
  recovery was needed. No reset, Guru Meditation or watchdog message occurred.

## Memory diagnostics

The boot line `PSRAM: present, N bytes` is `ESP.getPsramSize()`, which in the
framework `Esp.cpp` is `total_free_bytes + total_allocated_bytes` of the
`MALLOC_CAP_SPIRAM` heap: total usable PSRAM heap, not free PSRAM.

`Mem init:` lines are printed at allocation; a `Mem cycle N:` line is printed
after each cycle.

| Item | Value |
|---|---|
| PSRAM total / free before allocation | 8,386,295 B / 8,386,295 B |
| Feed buffer requested | 524,288 B |
| Buffer pointer (this firmware run) | 0x3d800908 |
| `esp_ptr_external_ram(buffer)` | YES |
| PSRAM free after allocation | 7,861,991 B |
| PSRAM allocation cost | 524,304 B including allocator overhead |
| Internal free before / after | 321,972 B / 321,972 B |

The 512 KiB buffer used PSRAM and did not consume internal heap.

During the captured cycles current free PSRAM was 7,846,811 B on every cycle.
The PSRAM minimum was 7,832,843 B through cycle 7 and 7,830,515 B from cycle 8
(a transient low-water mark; current free PSRAM did not decline). Internal
free heap stayed about 279,140-279,556 B, minimum internal 229,024 B
throughout, largest internal block 221,172 or 225,268 B. No progressive
memory loss was observed over the captured run. This does not prove a slow
leak is impossible.

## Captured cycles (re-uploaded run; cycle 1's detailed row was not recorded)

Captured cycles 2-9: 2 COMPLETE, 6 PARTIAL. All rows had buffer 0x3d800908,
external RAM YES, free PSRAM 7,846,811.

| Cycle | Result | Feeds | Trains | LEDs | Collisions | PSRAM min | Internal free | Internal min | Largest internal | Timeouts (cum.) |
|---|---|---|---|---|---|---|---|---|---|---|
| 2 | PARTIAL | 7/8 | 129 | 109 | 20 | 7,832,843 | 279,552 | 229,024 | 225,268 | 2 |
| 3 | PARTIAL | 7/8 | 131 | 103 | 26 | 7,832,843 | 279,140 | 229,024 | 221,172 | 3 |
| 4 | COMPLETE | 8/8 | 144 | 119 | 23 | 7,832,843 | 279,352 | 229,024 | 225,268 | 3 |
| 5 | PARTIAL | 7/8 | 129 | 106 | 21 | 7,832,843 | 279,356 | 229,024 | 221,172 | 4 |
| 6 | PARTIAL | 7/8 | 129 | 106 | 20 | 7,832,843 | 279,348 | 229,024 | 225,268 | 5 |
| 7 | COMPLETE | 8/8 | 143 | 120 | 22 | 7,832,843 | 279,556 | 229,024 | 225,268 | 5 |
| 8 | PARTIAL | 7/8 | 132 | 109 | 20 | 7,830,515 | 279,544 | 229,024 | 225,268 | 6 |
| 9 | PARTIAL | 6/8 | 87 | 74 | 13 | 7,830,515 | 279,548 | 229,024 | 221,172 | 8 |

Counters for this re-uploaded run (not merged with the earlier run): wifi 0,
secureConnectFailures 0, emptyCycles 0. The 10-COMPLETE-cycle target was a
validation goal and was not met; it was not worked around.

## Earlier firmware run (transient failures and recovery)

In an earlier run, cycle 1 was COMPLETE 8/8 and cycle 2 was PARTIAL 2/8
(counters wifi 3, secure 2, timeout 1) when Wi-Fi dropped; the user also
observed cumulative wifi 7, secure 1, emptyCycles 1 by cycle 8 followed by
complete cycles 5-8 (about 484-490 KB each). The board then recovered as
described above.

### Counter meanings (from `src/feed_transport.cpp`)

- `wifi` (`wifiUnavailable`): download attempts that found Wi-Fi unavailable
  (at open, or Wi-Fi found down after a failed GET), plus cycles skipped for
  Wi-Fi. Not a failed-association or reconnect count; one outage can add
  several (one per feed).
- `secure` (`secureConnectFailures`): TCP connect and TLS handshake/
  certificate failures are not distinguishable through HTTPClient.
- `emptyCycles`: a cycle that ran but processed zero feeds. Precondition-
  skipped cycles are not counted. The cause of the one observed empty cycle is
  inferred (one outage where all feeds failed), not confirmed.

## Unresolved: read-idle timeouts

The principal open observation is repeated read-idle timeouts on one or more
MTA feeds (1-2 per PARTIAL cycle). It is unknown whether the same feed repeats,
whether different feeds time out, whether the cause is Wi-Fi/network latency
or upstream MTA behavior, or whether the 8 s read-idle threshold is too
aggressive. Timeout policy is deliberately not changed in Phase 4K.

**Follow-up:** add a diagnostic-only transport report identifying the feed
short name, timeout type, bytes received before timeout, and elapsed
request/download time, before any timeout-policy change.

## Bench semantic diagnostic

The XIAO `M` command previously used the obsolete Phase 3 `logical_map` view.
That module was unreferenced elsewhere and was removed; `M` now uses the
Phase 4 semantic API. Observed on hardware:

`Semantic map validation: PASS; map LEDs 302 (tables 302), STATION 276 (tables 276), SEGMENT 26 (tables 26), registry entities 188, routes 18, physical LEDs 306 (non-map 4)`

## Final default builds (network disabled)

Compared with Phase 4J defaults: brooklyn-s3 RAM 34,240 B (unchanged), flash
306,121 B (-352 B); xiao-s3 RAM 33,116 B (-6,648 B), flash 286,205 B (+1,516
B). The XIAO flash change is an increase, not a reduction. The earlier
network-enabled XIAO hardware-test build is a different configuration.

## Not validated

Production PCB behavior, LED output, and any multi-hour soak.
