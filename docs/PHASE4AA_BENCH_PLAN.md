# Phase 4AA XIAO ESP-MQTT resource gate: bench plan (not yet executed)

Purpose: decide, with measurements, whether a persistent ESP-MQTT TLS session can coexist with the MTA HTTPS client, LAN UI and Wi-Fi on the ESP32-S3. This plan **supersedes the outline in memo 3** of [PHASE4AA_DECISION_MEMOS.md](PHASE4AA_DECISION_MEMOS.md). Design context: [PHASE4AA_FIRMWARE_INTEGRATION.md](PHASE4AA_FIRMWARE_INTEGRATION.md). Nothing here has been run; no results exist yet.

## Ground rules

- Environment `xiao-s3-network` only. XIAO is a bench target; never infer production wiring from it. No production PCB.
- `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0` throughout. No LED hardware attached.
- Joel must be present for flashing and credential entry. Firmware changes are confined to a throwaway bench branch; no merge to `main` without review.
- Credentials: the `brooklynmap` test account only, entered via local PowerShell secure prompt, never in source, Git, docs, chat or shell history. The `goldwein` account is not used by the device.
- Test topics only under `brooklyn/v1/brooklynmap/`. Clean retained topics afterwards.
- Publish-only; no subscriptions, no remote commands, no MQTT OTA.
- No broker, ACL, Apache or infrastructure changes.
- No invented numeric pass thresholds: Stage 1 baseline defines the reference; judgments are qualitative and recorded with the numbers.

## Instrumentation (bench branch only)

At each stage log over serial: `heap_caps_get_free_size`, `heap_caps_get_minimum_free_size`, `heap_caps_get_largest_free_block` for `MALLOC_CAP_INTERNAL` and `MALLOC_CAP_SPIRAM`; `uxTaskGetStackHighWaterMark` for the loop task and the MQTT task; MQTT state and last error category; MTA cycle state; elapsed time. Existing `printStatus` already prints most heap values.

## Stages

0. **Pre-flight.** Record branch, HEAD, clean tree, COM port, environment name, `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`, broker reachable, local credentials set, no tracked secrets (`git grep` scan).
1. **Baseline.** Current firmware, MTA cycling, LAN UI idle, no MQTT. Several cycles; record the heap table (reference for all later stages).
2. **Linked but disconnected.** Link esp-mqtt, create the client, do not connect. Confirms the footprint of linking and init.
3. **TLS idle.** Connect to `mqtt.goldwein.net:8884` with certificate-bundle verification, `MQTT_PROTOCOL_V_3_1_1`, keepalive 60 s, LWT retained QoS 1; stay connected idle with MTA paused.
4. **About 2 KiB publish.** Retained QoS 1 status; observe PUBACK, free-heap delta, fragmentation.
5. **About 4 KiB publish.** Near the 4,096 B ceiling; confirm the 4,096 B allocation boundary (internal vs PSRAM) and buffer sizing; confirm a >4,096 B payload is refused by the serializer, not sent.
6. **MQTT with MTA.** Persistent MQTT while MTA cycles run; record the minimum internal free and largest block during MTA TLS handshakes. Compare an MTA-handshake-while-connected against Stage 1.
7. **Multi-cycle.** Extended run (many MTA cycles and heartbeats). Watch for monotonic decline in free internal heap, largest block shrinking, stack high-water movement, MQTT task stalls.
8. **Network loss and reconnect.** Drop Wi-Fi/broker, then restore. Verify backoff and jitter, single attempt at a time, no reconnect storm, `offline` via LWT, recovery to `online` plus current status, no heap leak across stop/start cycles, no MTA starvation.
9. **TLS negative test.** Wrong hostname (e.g. connecting by IP or an alias not in the certificate) and an untrusted CA must **fail**; verify no insecure fallback and no data sent. Use only a bench build; never commit any bypass.
10. **Clean restore.** Return the XIAO to the known-good firmware, clear retained test topics, confirm no credentials in NVS dumps committed, tree clean, record final report.

## Results table (fill in per stage)

| Stage | Free internal | Min internal | Largest internal block | Free PSRAM | MQTT state | MTA state | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |

## Qualitative criteria

- **PASS:** MQTT connects and publishes reliably; MTA cycles complete as in baseline; internal heap and largest block stable across many cycles with no downward trend; reconnect behaves; TLS negatives fail closed; stack high-water has headroom.
- **PASS WITH CONCERNS:** works but margin is thin, a handshake overlap dips low, or a trend needs a longer soak; document mitigations (serialized connect windows, smaller buffers, delay connects until MTA idle).
- **FAIL:** MTA degrades or fails, heap/largest block declines monotonically, watchdog/panic, any insecure TLS acceptance, or reconnect storms. Then evaluate the fallbacks: ArduinoMqttClient, or connect-publish-disconnect windows (which gives up prompt LWT semantics and must be an explicit Joel decision).

## Outputs

A short results record under `docs/` with the filled table, the decision (persistent vs fallback), and the update to `RESUME.md`. The result gates any production firmware work.
