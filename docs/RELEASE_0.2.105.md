# Brooklyn Subway 0.2.105

This public release combines the monitoring, LED transport and diagnostic work
tested on the local production PCB. It contains no individual Wi-Fi or MQTT
credentials. Existing saved settings and provisioned monitoring credentials
are retained across the update.

## Remote monitoring

- Administrator-only **Advanced > Remote monitoring** settings support an
  individual device identity, broker and monitoring key. Monitoring defaults
  to On once provisioned; unconfigured maps do not open a monitoring connection.
- Verified-TLS, publish-only MQTT reports include firmware, uptime, train-feed
  health, LED counts, memory, Wi-Fi signal and observed disconnect counts.
  There is no MQTT command channel.
- LAN IPv4 and a cached public WAN IPv4 are reported when known. Public-address
  lookup uses verified HTTPS, is bounded, runs between feed cycles and can fail
  without stopping ordinary map operation.
- Expanded diagnostics report firmware-check timing and outcome, automatic
  update settings, output/quiet-hours state, the last feed error, outage duration
  and recovery, planned restart source, and the category/time of a saved setting.
- **Last button press** and **last local control use** distinguish button activity
  from knob turns. Times are unknown until an action after boot. Passwords,
  monitoring keys and submitted personal text are never included in these reports.
- A separate bounded diagnostic message uses the existing event topic. The
  original heartbeat format and publish-only broker permissions are preserved.
  Detailed reports normally update about once a minute or after changes;
  deliberate TLS/memory pauses can delay delivery.

The map's normal functions do not require the monitoring broker. Connection
retries run in a separate worker with backoff and memory safeguards. The new
collector support validates diagnostics independently of heartbeat freshness
and stores one latest diagnostic report per map; its SQLite schema is version 3.
Back up an existing collector database before upgrading the collector.

## LED transmission

Production LED output now uses a persistent ESP-IDF RMT transmitter, a complete
precomputed channel waveform, and an IRAM level-3 interrupt handler. This
addresses the brief scattered flashes observed with the previous driver,
including while monitoring was enabled. Transport errors stop output safely
and require a reboot. GPIO assignments, route/color settings and brightness
limits are unchanged.

## Home Assistant

The monitoring page's **Connection & diagnostics** area now includes
**Updates & restart**, **Display & controls**, **Saved settings**, and
**Feed & connection recovery** entries. The top status cards are unchanged.
Dashboard/discovery configuration is a separate Home Assistant setup step;
installing firmware does not configure a Home Assistant server.

## Validation and limits

- All four production/bench targets built successfully. Host checks cover
  telemetry/schema/privacy, bounded serialization, replay rejection, restart
  records, outage policy, OTA, web access, LED transport, controls and OLED.
- The local COM6 production board verified button/knob reporting, all four
  POWER modes, a saved setting, a planned POWER restart, quiet-hours entry and
  automatic recovery, and a signed OTA reinstall into the alternate partition.
  The OTA image was confirmed and Fresh 8/8 feeds recovered; Home Assistant
  received the resulting reports.
- No scattered LED flashes were seen in the user's short monitoring-Off/On
  trial. This is not an electrical waveform measurement or a long soak test.
- A live broker outage and deliberate OTA/feed failures were not induced.
  Those policies were checked with automated tests. Software-reported health
  does not verify physical LEDs, supply voltage or current.
- A manual upload can be rejected with HTTP 503 while a feed download is
  finishing. Retry after a few seconds; that busy rejection occurs before the
  installer starts. Downloading/staged MQTT phase delivery remains best effort
  and was not observed in the local OTA trial.
- This ordinary release does not enroll a previously unconfigured map. It also
  does not rotate or revoke existing broker credentials.

See [Remote monitoring](REMOTE_MONITORING.md) and
[expanded diagnostics](REMOTE_DIAGNOSTICS_0.2.105.md) for setup and field details.
