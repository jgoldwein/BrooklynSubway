# Expanded remote diagnostics — local test build 0.2.105

**Testing on the local COM6 map first. This build is not published, and Avi’s map remains on 0.2.104.**

The map still sends its normal heartbeat about every 30 seconds. A separate `kind: diagnostics` report goes to its existing `/event` topic about once a minute, or after a change (normally at most once every five seconds). Deliberate TLS and memory-protection pauses can delay delivery; an action's reported time is distinct from the report's arrival time. There are no new broker permissions, subscriptions, tasks or network connections. MQTT remains a read-only monitoring channel.

The additional information is:

- Automatic updates On/Off, the last actual firmware-check attempt and its result, the next check due time, available version and installation phase. The due time is a schedule, not a promise: checking also needs Wi-Fi, synchronized time and completed administrator setup.
- POWER mode, map/OLED output status, quiet hours, brightness, route selection and a reason when an output is unavailable. `waiting_for_data_or_safety` deliberately leaves an uncertain cause unresolved; the map cannot electrically verify emitted light.
- The last feed download error, its feed index (0–7), category, HTTP/TLS numeric result and age. A secure connection error does not separately establish whether DNS, a connection or TLS was responsible. It remains the **last recorded error** after recovery, not a claim of a current failure.
- Last Wi-Fi and MQTT outage duration and recovery time. Initial disconnection and deliberate MQTT pauses for other TLS work do not count as faults. Durations and counts cover the current boot.
- Restart source: web restart, long POWER press, firmware activation or firmware-default reset. A checked RTC record is consumed only on software reset; power loss, brownout, panic and watchdog resets cannot inherit a planned label. The existing heartbeat also reports the hardware reset reason and OTA health. A restart without a matching record is `unplanned_or_unknown`, not automatically a crash.
- The category and time of the last saved setting, plus a save counter for this boot. Only labels defined in the program are reported, never passwords, monitoring keys, Wi-Fi credentials, greeting text or form contents. Saving the same value still counts as a save. Switching monitoring off or removing its credentials tries to send a final report for up to 350 ms using an already connected client; delivery cannot be guaranteed when connectivity is absent.
- **Time since the last physical button press**, and separately time since the last local control use (button or knob turn). No individual button identity is transmitted. Values are unknown until a qualifying action after boot. Button sampling is debounced, and a button held during boot is not counted as a new press. The elapsed values use monotonic time; Home Assistant also derives approximate timestamps from the synchronized report time.

The collector validates a separate strict schema, rejects retained/replayed/mismatched diagnostic reports, and stores only one latest diagnostic report per map. Diagnostics do not advance heartbeat freshness or make a disconnected map appear healthy. Its SQLite schema upgrades from version 2 to 3; back up the database before upgrading. An older collector continues accepting heartbeats but does not understand these new reports.

## Home Assistant

The Merion **Brooklyn Subway** monitoring page displays these diagnostic entries below each map's existing **Connection & diagnostics** section. They are grouped as **Updates & restart**, **Display & controls**, **Saved settings**, and **Feed & connection recovery**. The top status cards, existing diagnostic tiles, connection history and compact Home card are unchanged. The entities also remain available under **Settings > Devices & services > MQTT > the map's device**; all raw report fields are attributes of **Last detailed report**. Button ages are labeled “at report” because they do not tick every second in the firmware report. Source timestamps and availability must be considered when inspecting an old report.

The configuration builder is [ha_diagnostics.py](../tools/telemetry/ha_diagnostics.py). It preserves existing MQTT discovery components and does not contain credentials or publish anything itself. Both maps' HA discovery configurations include the new diagnostic entries. COM6 supplies them on 0.2.105; Avi's new entries are marked as waiting for firmware 0.2.105 or later. Registering the entries in HA does not change Avi's installed firmware or settings.

## Validation and operator checks

Automated checks cover bounded serialization, null/undersized buffers, JSON schema, privacy fields, duplicate and stale boot rejection, latest-only storage, outage recovery, restart-record corruption and one-time consumption, cadence and PUBACK bookkeeping. Existing heartbeat, web, OTA, LED transport, live display, controls and OLED tests are also run.

On the test map, turn the knob and press it briefly. Check that the two activity times become known. Press POWER four times with about ten seconds between presses; expected modes are 1 (LEDs off), 2 (OLED off), 3 (both off), 0 (both on). Save a setting without changing it, then perform an ordinary web restart and confirm the reported save category and restart source. A short temporary quiet-hours window can verify off/dim and recovery; restore the original schedule afterward. Network-outage and OTA failure policies are host-tested; do not disturb the household router or Avi’s map for a local test.

A byte-for-byte application readback is kept locally before the test upload. COM6’s existing NVS settings and individual monitoring credentials are retained. No per-map credentials are compiled into the ordinary 0.2.105 image.

## Local trial results — November 20, 2026

The COM6 trial verified button and knob activity reporting, all four POWER modes, the saved OLED-brightness category and timestamp, and a planned long-POWER restart. Quiet hours disabled map output at 6:13 PM and restored it automatically at 6:18 PM New York time; both transitions were reported through MQTT. The user then chose a normal schedule of 10:30 PM–6:30 AM, with map lights Off during quiet hours.

A signed manual reinstall of 0.2.105 booted the alternate `app1` partition, reported `firmware_update`, progressed from `health_check_pending` to `confirmed`, and recovered to Fresh 8/8 feeds. Home Assistant received the confirmed OTA state and recovered feed status. One initial upload received the expected busy HTTP 503 before the installer started; retrying succeeded. Downloading/staged phase delivery was not observed through MQTT during this trial, so those phases are not claimed as live-verified.

A real broker outage and deliberate feed/OTA failures were not induced on the live board; their policies were checked by automated tests. The release remains unpublished, and Avi’s board remains on 0.2.104. Home Assistant's top status card layout is unchanged; the additional parameters appear as diagnostic entries below the existing diagnostics on the dedicated monitoring page.
