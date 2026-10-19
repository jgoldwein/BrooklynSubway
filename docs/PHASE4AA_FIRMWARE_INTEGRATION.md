# Phase 4AA embedded MQTT integration review (read-only design; no firmware changed)

Scope: how a future `TelemetryService` would integrate with the existing firmware.
Produced by reading `src/`, `include/`, and the installed PlatformIO framework
package. **Nothing was modified, built, or flashed.** Statements marked
*(bench)* must be measured on the XIAO before being relied on
([PHASE4AA_BENCH_PLAN.md](PHASE4AA_BENCH_PLAN.md)). Frozen contracts:
[PHASE4AA_REMOTE_TELEMETRY_DESIGN.md](PHASE4AA_REMOTE_TELEMETRY_DESIGN.md),
[PHASE4AA_MQTT_TOPIC_SCHEMA.md](PHASE4AA_MQTT_TOPIC_SCHEMA.md), `docs/schemas/`.

## 1. How the firmware is structured today (facts)

- Single Arduino `loop()` in [main.cpp](../src/main.cpp): `controls::update()`, `ota::healthService()`, `networkService.update(millis())`, `consumeCompletedCycle()` -> `operationalDisplay.completeCycle()`, `updateLogicalDisplay()`, then it builds a `web::StatusSnapshot` and calls `networkService.setWebStatus()`, finishing with `delay(5)`. There are no application FreeRTOS tasks.
- [network_service.cpp](../src/network_service.cpp): `NetworkService::update()` is a state machine over `network::State` (`Disabled, NoCredentials, Provisioning, WifiConnecting, TemporarilyUnavailable, TimeSyncing, Ready`). `Ready` means Wi-Fi associated **and** SNTP valid (`time(nullptr) >= kMinValidUnixTime`, set in `serviceTime()`); it falls back to `WifiConnecting` when `WiFi.status() != WL_CONNECTED`.
- In `Ready`, `pollNow()` runs the whole MTA cycle **synchronously inside `loop()`** (`transport::runPollCycle`; per-feed connect/handshake 8 s, read-idle 8 s, feed deadline 12 s, cycle budget 60 s; `kDefaultNetworkConfig`). `HttpsFeedConnection` uses one `WiFiClientSecure` at a time, `setReuse(false)`, `setCACertBundle(rootca_crt_bundle_start)`, never `setInsecure()`. The 512 KiB feed buffer is a PSRAM `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` allocated in `allocateBuffer()`.
- `NetworkService` already samples what telemetry needs: `heap_caps_get_free_size/minimum_free_size/largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_SPIRAM)` (`printStatus`, `serviceLanWebUi`), `WiFi.RSSI()`, `diagnostics::resetReasonName()`, and the network-diagnostics `Snapshot` (DNS health).
- The LAN web UI (Phase 4R/4V) is a plain-HTTP `WebServer` also running in `Ready`; it can trigger `ESP.restart()` after `delay(250)` (network_service.cpp, `serviceLanWebUi`). OTA (Phase 4W, [ota_esp.cpp](../src/ota_esp.cpp)) uses `esp_ota_*` with rollback health gating via `ota::healthSetupComplete()/healthService()`; it has no public state getter today.
- NVS namespaces in use: `bkmap_wifi` (wifi_portal.cpp `kNs`), `bkmap_cfg` (`config::kNvsNamespace`), and admin auth (admin_auth_esp.cpp). The design reserves `bkmap_mon` for monitoring identity/credentials.
- `esp_fill_random()` is already used (admin_auth_esp.cpp); `esp_reset_reason()` already mapped in diagnostics.cpp.

## 2. Recommended module boundary

**A separate `TelemetryService` (new `include/telemetry_service.h` / `src/telemetry_service.cpp`), driven from `loop()` in `main.cpp`, not owned by `NetworkService`.**

Reasons: `NetworkService` is already the largest module (about 1,100 lines) and owns Wi-Fi, SNTP, MTA transport, the LAN UI and DNS; adding a second TLS client lifecycle there would couple MTA correctness to telemetry bugs. A separate service only consumes read-only inputs (`network::State`, snapshot values) and exposes `update(nowMs, inputs)`, `enqueueEvent(...)`, `requestShutdown(deadlineMs)`. MTA remains the priority owner of Wi-Fi/SNTP; telemetry never starts Wi-Fi, SNTP, or the portal. Pure logic (serialization, queue, backoff, sequence/boot-id) lives in a host-testable class with no ESP headers (the repository's existing pattern: pure logic + `*_esp.cpp` adapter), so most of it is unit-tested by a new `tools/run_telemetry_firmware_tests.py`-style runner before any flashing.

Responsibilities: connection lifecycle, status/event serialization, bounded publish queue, QoS-1 acknowledgement tracking, reconnect scheduling, availability/LWT.
Explicitly **not** owned: MTA parsing, map semantics, LED rendering, OTA signing/installation, Wi-Fi provisioning, LAN admin authentication. No MQTT subscribe path and no application receive callback beyond CONNACK/PUBACK-level events.

### Gating (all must hold, otherwise disconnected and idle)

`config monitoring enabled` (NVS `bkmap_mon`, default off) AND `networkService.state() == State::Ready` (implies Wi-Fi + valid SNTP, so **no valid SNTP => no MQTT TLS**) AND identity/credentials present AND not in Provisioning AND no OTA upload in progress AND internal-heap guard satisfied *(threshold set only after bench measurement)*.

## 3. Exact integration points (existing symbols)

| Need | Existing hook | Needed (future, additive) |
| --- | --- | --- |
| Network readiness / Wi-Fi / time | `networkService.state()` (public), `time(nullptr)`, `kMinValidUnixTime` | none |
| Telemetry snapshot assembly | `main.cpp` `loop()` already builds `web::StatusSnapshot` from `operationalDisplay` and `networkService` | build a separate `telemetry::Snapshot` in the same place; **do not** reuse `StatusSnapshot` directly because it carries privacy-excluded fields (`ssid`, `ip`, `gateway`, `dns*`) |
| Uptime / reset reason | `millis()`, `diagnostics::resetReasonName()` | map names to schema enum (`power_on, software, panic, watchdog, brownout, ...`) |
| Heap / PSRAM | `heap_caps_*` (as in `network_service.cpp`) | none |
| RSSI / DNS health | `WiFi.RSSI()`; network-diagnostics `Snapshot` (inside `NetworkService`) | tiny const getters or a copy through `setWebStatus` |
| MTA state | `consumeCompletedCycle()` (already consumed in `main.cpp`), `trainPipeline.counters()`, `operationalDisplay.frame()` (`resolvedTrains`, `occupiedLeds`, `collisionLeds`) | capture `CycleResult` (`attempted`, `processed`, `processedMask`) into the snapshot at the existing consume site |
| Display state / brightness / routes | `operationalDisplay.state()`, `networkService.brightness()/routeVisibilityMask()` | none |
| Display events | `reportDisplayTransition()` in `main.cpp` sees every transition | call `telemetry.enqueueEvent(display_degraded / display_recovered / display_stale / display_no_data)` there |
| Integrity | `semantic::selfCheck()`, `systemHealthy` (main.cpp) | store the results once at boot |
| Safety | `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT`, `physical_output_safety.h` | read-only copy of arming/controller state |
| OTA state/events | `ota::healthSetupComplete/healthService`, `esp_ota_*` in ota_esp.cpp | read-only state getter (`idle, pending_verification, confirmed, rollback`) and event callbacks; no OTA code is moved |
| Reboot / shutdown | `ESP.restart()` after `delay(250)` in `serviceLanWebUi`; OTA reboot path | call `telemetry.requestShutdown()` before restart; bounded (<= ~1 s) best effort, otherwise rely on the LWT |
| Firmware version / build id | none exist (`web::kFirmwareId` is the string "Phase 4W") | build-time defines for `version`, `build_id`; `firmware.image_sha256`/`active_partition` optional |
| LAN status display | `web::StatusSnapshot` + `lan_web_ui.cpp` | add telemetry fields (enabled, connection state, last publish age, last error category) behind the existing Phase 4V protections; enabling/disabling and credential entry only via the authenticated UI |

## 4. `esp-mqtt` viability (from the installed toolchain, read-only)

Installed: `framework-arduinoespressif32` package version 4.20017.x (Arduino-ESP32 2.0.x), ESP-IDF **4.4.7**; `esp32s3/sdkconfig`.

- **Present without any `platformio.ini` change:** `tools/sdk/esp32s3/include/mqtt/esp-mqtt/include/mqtt_client.h` is on the include path (`platformio-build-esp32s3.py`) and `-lmqtt` is already in the link list; `libmqtt.a` exists. Static linking only pulls it in when referenced. *(Compilation not attempted; the first compile happens in the resource-gate phase.)*
- **IDF 4.4 API shape (flat config struct, not the IDF 5 nested form):** `esp_mqtt_client_config_t` with `uri`, `client_id`, `username`, `password`, `lwt_topic/lwt_msg/lwt_msg_len/lwt_qos/lwt_retain`, `keepalive`, `disable_clean_session`, `protocol_ver = MQTT_PROTOCOL_V_3_1_1`, `disable_auto_reconnect`, `reconnect_timeout_ms`, `buffer_size`, `out_buffer_size`, `task_prio`, `task_stack`, `crt_bundle_attach`, `cert_pem`, `skip_cert_common_name_check` (must stay false/unset). Functions: `esp_mqtt_client_init/start/stop/destroy/reconnect/disconnect/publish/enqueue/register_event/get_outbox_size`. Events: `MQTT_EVENT_BEFORE_CONNECT/CONNECTED/DISCONNECTED/PUBLISHED(msg_id)/ERROR/DELETED`.
- **sdkconfig facts:** `CONFIG_MQTT_PROTOCOL_311=y`, `CONFIG_MQTT_TRANSPORT_SSL=y`; `CONFIG_MQTT_SKIP_PUBLISH_IF_DISCONNECTED` **not set** (see risks); `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` (FULL bundle, same bundle the MTA client uses); `CONFIG_ESP_TLS_INSECURE` not set.
- **TLS verification:** use `crt_bundle_attach = esp_crt_bundle_attach` with `mqtts://mqtt.goldwein.net:8884`; hostname is verified against the URI host. Never set `skip_cert_common_name_check`; never use `cert_pem = NULL` without a bundle. The bench must confirm the broker's certificate chain validates against the bundle *(bench)*.
- **Last Will:** `lwt_topic = brooklyn/v1/<id>/availability`, `lwt_msg = "offline"` (the schema JSON form), `lwt_qos = 1`, `lwt_retain = 1`.
- **QoS 1 / retained:** `esp_mqtt_client_enqueue(client, topic, data, len, qos=1, retain, store=true)` is documented as the non-blocking form (work done in the MQTT task); `esp_mqtt_client_publish` may block on the client lock. Use `enqueue`. `MQTT_EVENT_PUBLISHED` carries `msg_id` for PUBACK tracking.
- **Conclusion: direct `esp-mqtt` integration looks compatible with the existing framework. Recommendation stays: `esp-mqtt` preferred, ArduinoMqttClient fallback, PubSubClient rejected** (QoS 1 publish unsupported). Nothing found changes this ordering; the open questions are RAM, not API availability.

## 5. Threading and callbacks

`esp-mqtt` runs its own FreeRTOS task (default priority 5, stack 6144 B; core not pinned since `CONFIG_MQTT_TASK_CORE_SELECTION_ENABLED` is unset; Arduino `loop()` runs on core 1). Consequences:

- Keepalive/PING continue while `loop()` is blocked inside an MTA cycle (up to 60 s), which is a benefit: no spurious broker disconnects.
- Event callbacks run in the MQTT task: they must only set atomics/flags or post into a small fixed ring; all serialization, NVS access, logging and state decisions happen in `TelemetryService::update()` on the loop task.
- `esp_mqtt_client_stop/destroy` must not be called from the event handler (per API docs); the service requests them from `update()`.

## 6. Telemetry snapshot design

`struct telemetry::Snapshot` (plain values, no pointers into other modules, fixed-size char arrays), copied once per heartbeat from existing state by `main.cpp`. Fields are exactly those of `brooklyn-telemetry-v1.schema.json` (`firmware`, `system`, `network` [connected, RSSI, DNS health only], `mta`, `display`, `integrity`, `configuration`, `safety`, bounded `extensions`). **Excluded by construction:** SSID/BSSID, IP/subnet/gateway/DNS addresses, MAC, passwords/tokens, trip/vehicle/GTFS identifiers, stored configuration values. The serializer is a bounded writer into a fixed buffer (no `String`, no heap growth); it fails closed (drops the message and counts `serialize_overflow`) if output would exceed 4,096 bytes. Measured sizes on the host: typical heartbeat about 1.4 KB, schema worst case about 2.3 KB, ceiling 4,096 B (design target: normal well under 2 KiB). `device_time_utc` is `null` until SNTP is valid.

## 7. `boot_id` and `sequence`

- `boot_id`: 128 random bits from `esp_fill_random()` at boot, formatted as UUIDv4 (matches the frozen schema); never persisted in NVS; new every reset. Collision probability is negligible (2^-122 per pair).
- `sequence`: a single `uint64_t`-range counter per boot (schema allows up to 2^53-1), starting at **1**, shared by status and event messages, incremented **once per logical message at serialization time** (not per retry; a QoS-1 redelivery re-sends the same bytes with the same sequence, which the collector dedups by `(device_id, boot_id, sequence)`). A dropped message leaves a visible gap, which the collector tolerates.
- Wraparound: unreachable (2^53 messages at 1/s is about 285 million years). Defensive rule: if the counter would exceed 2^53-1, generate a new `boot_id`, restart at 1, and emit a `boot` event with the same `reset_reason`.

## 8. Event queue (RAM-only, bounded)

- Fixed ring of **16** pre-allocated event slots (small structs: type, severity, optional bounded attributes), plus one slot for the current status. No NVS/flash queue and no unbounded retry backlog. At most **one** QoS-1 message in flight (so the library outbox never exceeds one entry).
- Sequence numbers are assigned when a message is serialized for sending, so ordering is deterministic and gap-free for what was actually sent.
- **After (re)connect:** (1) publish retained `online` to availability, (2) publish the **newest current status** (retained), (3) drain queued events oldest-first.
- **Overflow policy:** when full, drop the oldest *low-value* entry first (display/Wi-Fi flap events and coalescible duplicates, with a `dropped_event_count`), keeping high-value events (`boot`, `unexpected_reset`, `brownout`, `watchdog_reset`, `ota_*`, `physical_safety_fault`, `configuration_invalid`, `heap_critical`); if all are high-value, drop the oldest. Overflow never blocks `loop()`; the next status heartbeat reports the drop count in a bounded extension field.
- While disconnected, status is not queued (only the latest is rebuilt on connect); events accumulate up to the bound. `CONFIG_MQTT_SKIP_PUBLISH_IF_DISCONNECTED` is **not** set in this framework, so the service must never call `enqueue/publish` unless it knows the client is connected, otherwise the library outbox would grow.

## 9. Reconnect scheduling

- Set `disable_auto_reconnect = true`; the service owns all reconnect timing (a library-driven fixed retry cannot express exponential backoff with jitter, and cannot be gated on `network::State::Ready`/SNTP).
- Backoff: start 5 s, double each failed attempt, cap about 15 min, full jitter of +/-20%, from `esp_random()`; reset the backoff only after the connection has stayed up for a stability window (e.g. 5 min). One attempt at a time; no reconnect storm after broker or Wi-Fi outage because attempts depend on the timer, not on events.
- Each attempt is `esp_mqtt_client_start()` and, after a disconnect or failure, `esp_mqtt_client_stop()` from `update()`; destroying/stopping between attempts releases the mbedTLS contexts (important for internal RAM, see risks). *(bench: confirm `stop/start` cycles do not leak and that `esp_mqtt_client_reconnect()` is not needed.)*
- TLS handshake timeout and network timeout must be bounded (`network_timeout_ms`) and may never delay `loop()`, because the connect happens in the MQTT task.
- Telemetry never retunes MTA constants (`kDefaultNetworkConfig`); if internal heap is low, telemetry yields (disconnects after publishing `offline`, stays down) rather than MTA.

## 10. Expected RAM-risk areas (to be measured, not assumed)

1. **TLS in internal RAM.** The bundled sdkconfig has `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=y` (not external), `CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN=16384`, and `CONFIG_MBEDTLS_ASYMMETRIC_CONTENT_LEN` off. Each TLS session therefore keeps up to 16 KiB in + 16 KiB out record buffers plus handshake state in **internal** heap (on the order of 40 KB per session by configuration; actual figure is a bench measurement). A persistent MQTT/TLS session coexists with every MTA `WiFiClientSecure` session (one at a time, `setReuse(false)`), plus the LAN `WebServer`, mDNS, and Wi-Fi. The prebuilt libraries cannot be reconfigured without changing the framework build, so the options are: measure, then choose among persistent connection, serialized connect/publish/disconnect windows (loses prompt LWT semantics), or ArduinoMqttClient over a shared `WiFiClientSecure` strategy.
2. **Handshake peak.** The MQTT handshake (reconnect) overlapping an MTA handshake is the worst case; handshake peaks exceed steady state. The service should not start a connect attempt while an MTA feed is open (needs a read-only "poll active" flag) *(bench)*.
3. **Allocation threshold:** `CONFIG_SPIRAM_USE_MALLOC=y` with `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096`: allocations up to 4096 B go to internal heap, larger to PSRAM. A near-4 KiB QoS-1 message plus MQTT header crosses that boundary; where the stored outbox copy lands and what buffer sizes are required is a bench question.
4. **Buffers:** `buffer_size` (receive) can stay small since the device receives only CONNACK/PUBACK; `out_buffer_size` and large-publish behavior for 2-4 KiB payloads must be verified.
5. **MQTT task stack** (default 6144 B, internal) and TCP/lwIP buffers.
6. **Fragmentation** of the largest internal free block after repeated connect/teardown cycles, and Wi-Fi reconnect allocations.
7. **QoS-1 in-flight storage** and library outbox (bounded to one entry by design; DELETED events must be handled).

No numeric RAM thresholds are invented here; they are set from the baseline in the bench plan.

## 11. Interactions and invariants

- **MTA priority:** telemetry never blocks `loop()`, never touches the PSRAM feed buffer, never retunes MTA timeouts; MQTT task work is invisible to MTA except for RAM and radio airtime.
- **Wi-Fi provisioning:** on `State::Provisioning`/`NoCredentials`, telemetry disconnects (publish `offline` only if the link is still up) and stays idle; Wi-Fi recovery (10 s rotary hold) is unchanged.
- **LAN UI / auth:** only the existing Phase 4V-protected UI may enable monitoring or enter credentials; credentials are write-only in the UI, never echoed, never logged (serial diagnostics print categories, not secrets).
- **OTA:** no MQTT-triggered OTA, no remote commands, no subscriptions; OTA signing untouched; telemetry reports `ota_state` read-only and emits `ota_pending_verification/confirmed/rollback` events.
- **Hardware safety:** nothing here touches GPIO, `LED_ENABLE`, or physical output; `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT` stays 0 for bench work.
- **Diagnostics:** the first firmware phase adds serial diagnostics for the new subsystem (state, last error category, heap numbers) per project rules.

## 12. Decisions flagged for Joel

1. Whether persistent MQTT is acceptable if the bench shows low internal RAM margin (fallback options in section 10.1).
2. Device ID format (see [PHASE4AA_PROVISIONING_AND_IDENTITY.md](PHASE4AA_PROVISIONING_AND_IDENTITY.md)); affects schema and topic strings before firmware is written.
3. Whether to add a read-only OTA state getter and build-time version/build-id defines in the first firmware phase.
