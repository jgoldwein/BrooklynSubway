# COM6 morning recovery investigation — 0.2.79

Reported November 16, 2026: map stayed dark after quiet hours; OLED remained
visible. No user settings were changed before evidence capture.

## Evidence before recovery

- COM6 0.2.78: quiet hours inactive and clock synchronized, but NoData, 0/8
  feeds, LED output off. More than 4,000 secure-connect failures since boot.
- Serial and runtime diagnostics repeatedly reported TLS -32512 (allocation
  failure at mbedtls_ssl_setup). About 79 KB internal memory was free, but
  the largest block was only 32,756 bytes. PSRAM had about 7.8 MB free.
- Reset reason panic; two unexpected warm resets recorded. Previous checkpoint
  retained about 7.5 hours of uptime. A panic backtrace was not available in
  the preserved snapshot; its cause is not established by the allocation fix.
- COM5 0.2.50 remained Fresh 8/8 with no unexpected resets. Its scheduled
  eight-hour baseline capture completed with zero boot changes. Partial feed
  cycles and request errors occurred; it was not a perfect network run.

## Changes

A failing host regression reproduced quiet-hours end with NoData: the pending
resume request was erased. The controller now retains that explicit pending
request through unavailable/stale data and resumes only when fresh usable data
returns. POWER-off, provisioning/recovery, unhealthy hardware and actual output
failures still cancel automatic intent.

For production network builds, install the pinned SDK's
mbedtls_platform_set_calloc_free callbacks once before network startup.
Allocations at least 4096 bytes prefer PSRAM, then fall back to internal RAM;
small allocations retain internal RAM. Allocation multiplication is checked for
overflow and memory is zero initialized. Both memory types use heap_caps_free.
This covers feed/weather/update TLS without altering certificate validation,
TLS versions, trust keys or firmware signatures. No new dependency or GPIO.

The pinned SDK defaults mbedTLS to internal memory despite plentiful PSRAM.
References: [Espressif 4.4.7 memory allocation options](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/kconfig.html#config-mbedtls-mem-alloc-mode)
and the installed SDK mbedtls/platform.h / mbedtls/esp_config.h.

## Validation and limits

All 35 host suites and four targets pass. New allocator tests cover installation
retry/idempotence, routing, zero initialization, fallback, failure and overflow.
The quiet regression failed before the change and passed afterward.

Private 0.2.79 was loaded only onto serial-verified COM6 using USB because its
existing TLS failure also prevented GitHub release checks. Hashes verified;
settings retained. ELF retained locally with the frozen image for backtrace
symbolization. COM5 was not modified.

Two initial post-fix cycles completed 8/8 with live output. Serial showed 36
external TLS allocations, zero internal fallbacks and zero allocation failures.
Minimum internal heap was about 63 KB and largest free block about 63 KB.
This demonstrates initial data recovery, not overnight panic resolution.
A short real quiet-hours end test and extended capture remain necessary.
The candidate has not been published as a stable GitHub release.

Local evidence: captures/COM6-quiet-end-2026-11-16.json,
captures/COM6-runtime-diagnostics-quiet-end.json,
captures/COM6-quiet-end-passive-serial.txt and captures/startup-079-COM6.txt.
