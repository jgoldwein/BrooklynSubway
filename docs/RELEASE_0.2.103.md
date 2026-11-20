# 0.2.103 - Individual MQTT monitoring provision

## Added

- Advanced > Remote monitoring: administrator-only server/port, individual
  device UUID and monitoring key, map label, On/Off and credential removal.
- Monitoring defaults to On once provisioned. Unconfigured maps do not start
  the sender or open a monitoring connection. Existing maps need their own
  broker account and one local enrollment before they can report.
- Verified-TLS MQTT status about every 30 seconds: current display/feed state,
  uptime, firmware, clock, Wi-Fi signal, memory, train/LED counts and observed
  Wi-Fi/broker disconnect counts with occurrence uptime. Reboots reset counts.
- Current local sender state and server-acknowledgment age refresh every five
  seconds without refreshing the settings form. Sign-in returns to this page.
- Private enrollment helper creates unique keys and publish-only ACL fragments.
  Generated credentials are not part of firmware, source or release assets.
- Separate private-update builder can prepare a signed, unit-bound enrollment
  image for a shipped map. Such an image contains its key and is never a public
  release asset. One-time seed handling preserves existing choices and respects
  later credential removal/default reset.

## Behavior

- Separate low-priority bounded sender, reconnect backoff, TLS certificate and
  hostname verification, no subscriptions or remote-control channel.
- Memory/readiness gates and stale-application-snapshot suppression; monitoring
  relinquishes its connection during update checks and firmware installation.
- Medium TLS allocations (1024 bytes and larger) prefer PSRAM to keep internal
  RAM available for concurrent feed/monitoring TLS. Memory-pressure suspension
  waits a minute before retrying rather than rapidly reconnecting.
- One atomic NVS record. Updates/restarts retain it; firmware defaults erase it.
- The saved monitoring key is never rendered back. No built-in developer or
  shared bench credential is used. Default endpoint is mqtt.goldwein.net:8884.
- MQTT reception works without an Apache dashboard. Broker accounts/ACLs and
  receiver setup are separate operator steps; firmware does not install a server.

## Limits and validation

Software-reported health is not physical LED, voltage or current verification.
Retained availability is not proof of a live map; use fresh heartbeat timestamps
and last-seen age. Disconnects observed between samples may be missed, and an
outage can only be reported after reconnection. Wi-Fi names, network addresses,
passwords and individual train IDs are excluded; the chosen map label is shared.

Host checks cover configuration/credential handling, access policy, schemas,
telemetry scheduling/recovery, one-time seed interruptions/unit matching, reset
and signed OTA. Production and bench targets build. Live Mosquitto 2.0.18
preflight confirmed verified TLS authentication, own-namespace publishing allowed
and foreign publishing denied (MQTT 5 PUBACK codes 16/135).

COM6's enabled test held one verified-TLS MQTT connection with regular QoS-1
status acknowledgments while MTA cycles completed 8/8. At 247 seconds its
monitoring counters showed one connection, no reconnects or missed acknowledgments,
and eight status reports; internal heap minimum was 27,960 bytes, with no TLS
allocation failures. This is short-run software evidence, not prolonged stability
or independent physical-output verification. Independent subscriber inspection
and private-image hardware commissioning remain separate steps.
