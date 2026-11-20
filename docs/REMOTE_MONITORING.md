# Remote monitoring

The production monitoring provision is being prepared for firmware 0.2.103.
It is not proof that a particular map has been enrolled or has reported.
Older Phase 4AA design/bench notes describe earlier milestones; this guide
describes the provisioned production path.

## What it does

Once a map has its individual credentials, monitoring defaults to **On**.
The administrator can turn it off at **Advanced > Remote monitoring**.
Without credentials, it opens no monitoring socket or sender task.

About every 30 seconds the map publishes a small status report to an existing
MQTT broker over verified TLS. Default: `mqtt.goldwein.net:8884`.
Server and TLS port are configurable. Plaintext MQTT and anonymous publishing
are not supported. Reports never control the map.

The report contains:

- Current application/display state and the time the report was generated.
- Uptime since the last boot, a random boot ID and reset reason.
- Firmware version, Wi-Fi signal, synchronized-clock state and memory.
- MTA feeds received out of eight, age of the last complete cycle, train and LED counts.
- Observed Wi-Fi and monitoring-connection losses, with their occurrence times
  measured in seconds since boot. Counts start when the monitoring worker starts
  after each boot. An initial failed connection is not a disconnect. Short Wi-Fi
  interruptions between worker samples may not be observed.
- Logical brightness and route mask, and reported LED-output enable state.

An indication that LED output is enabled is a software report, not an electrical
measurement or proof that individual LEDs are illuminated. No current sensor is
installed. Clock/DNS health that cannot be measured is reported as unknown.

Reports exclude Wi-Fi SSIDs/BSSIDs, MAC/IP/gateway/DNS addresses, passwords,
support email, personal greetings, and individual train/trip IDs. The map label
you choose is included. The broker can see the connection's source IP normally;
omitting it from the payload does not hide it from the server.

## Receive reports with your existing MQTT client

An Apache dashboard is optional. You can first inspect messages using an existing
read-authorized MQTT client, such as the broker administrator's client, or run
the repository collector. No dashboard deployment or router change at the map's
home is required. Existing broker port forwarding/TLS must already be working.

Each map uses a random UUIDv4. Its username and client ID are `bkmap-<UUID>`.
Subscribe with a separate read-authorized account to:

```
brooklyn/v1/bkmap-<UUID>/status
brooklyn/v1/bkmap-<UUID>/event
brooklyn/v1/bkmap-<UUID>/availability
```

`status` is retained JSON; `event` is non-retained JSON. Availability is a retained
plain UTF-8 `online` or `offline` string. Do not treat a retained `online` message
as proof that the map is currently healthy. Check for NEW heartbeats, their UTC
timestamps, boot IDs and sequences. If no new report arrives for 75 seconds,
show Late/Unknown; after 120 seconds, show Offline. An explicit `offline` Last
Will also marks it offline. These thresholds are for the default 30-second cadence.
The existing collector applies these rules and does not make retained replay live.

For an account/ACL preflight, the developer can run
`py tools/telemetry/check_account.py --settings dist/monitoring-enrollment/avi-map/device-settings.json`.
It reads the key locally, never prints it, and uses a separate MQTT 5 diagnostic
client to check own-namespace permission and foreign-namespace denial. It sends
only two non-retained diagnostic event payloads, not device heartbeats. Broker
permission checks are distinct from observing real device reports in a subscriber.

## Set up an individual map

For the developer/broker administrator:

1. From the repository root, create a private bundle:

   ```powershell
   py tools/telemetry/enroll.py --name "Avi map" --out dist/monitoring-enrollment/avi-map
   ```

2. Open the generated `SETUP.txt`. `device-settings.json` contains the PRIVATE
   key. It must not be uploaded to GitHub or included in a firmware image.
3. Back up `/etc/mosquitto/passwd` and the 8884 listener's ACL. Use
   `mosquitto_passwd /etc/mosquitto/passwd bkmap-<UUID>` to add the device and paste
   its generated key at the hidden prompts. Do not use `-c`, which replaces the
   password file, or `-b`, which exposes a password in process arguments.
4. Append the generated `broker-acl.fragment` to that listener's ACL, preserving
   existing users. It allows publishing only the map's own three topics; it does
   not allow subscriptions or publishing other devices' reports.
5. Reload the broker and check its log for errors. This tool does not edit or
   reload a live server automatically. For Mosquitto 2.0.18, the existing
   `per_listener_settings true` and listener-specific ACL configuration is valid.

### Where the files are, editors and server warnings

The enrollment bundle is created on the computer where you run `enroll.py`.
For example, a Windows bundle stays under the repository's
`dist\monitoring-enrollment\avi-map` folder. Open its JSON file to read the
generated key. You do not copy that JSON file into `/etc/mosquitto`.
The password and ACL changes are made in the Linux server's existing files.
Start with a separate test-map bundle before enrolling a shipped map.

For the observed GNU `cp` version on mythbackend, use `cp -p --update=none`
when making a backup without overwriting an existing backup. `cp -pn` may print
a warning about future behavior; that warning does not itself mean the copy
failed. Check that the backup files exist before changing originals.

If editing the ACL with **vi/vim**, append the new `user` and three `topic write`
lines, then press **Esc**, type **`:wq`**, and press **Enter** to save and exit.
Lines containing only `~` are unused editor lines; do not add them to the file.
For **nano**, save with **Ctrl+O**, **Enter**, then exit with **Ctrl+X**.
After saving, run `systemctl reload mosquitto`, then
`systemctl status mosquitto --no-pager` and check the broker log. Existing
listeners, certificates and ACL users remain unchanged.

Some `mosquitto_passwd` versions warn that non-root password-file ownership will
be refused by a future version. This is distinct from a failed password update.
Do not blindly change ownership or permissions while setting up monitoring:
first inspect `stat /etc/mosquitto/passwd` and the broker service's runtime user,
and ensure the broker can still read its password file. Do not paste the password
file contents into chat or a support report.

For the map owner:

1. Install the monitoring release and connect to the map's normal web page.
2. Open **Advanced > Remote monitoring** and sign in.
3. Enter the server, port, label, Device ID and key supplied by the administrator.
4. Leave **On** selected and press **Save monitoring**. No reboot is needed.
5. Look for a server acknowledgment, then have the administrator confirm that a
   NEW status message actually arrives under the correct topic.

Mosquitto PUBACK does not prove an ACL allowed delivery, so both checks matter.
Do not use the same credentials on two boards simultaneously: their MQTT client
IDs would conflict. Use a separate bundle for the local test board.

The local settings page is HTTP. Use it only on a trusted home network. The
monitoring key is never rendered back. A blank key keeps the existing key only
when the host, port and identity are unchanged. NVS is not flash-encrypted:
sufficient physical access can extract a key. Each key can be revoked separately
on the broker. Restart and signed updates preserve the settings; **Reset to
firmware defaults** removes them. A 10-second knob hold for Wi-Fi setup does not
erase monitoring credentials.

## Preconfigured private updates

For an already shipped map, the developer can prepare a private, signed update
with that unit's individual monitoring settings. Its owner installs the private
`.bkota` once through the normal signed updater; no monitoring form is needed.
The matching broker account and ACL must exist first. Later ordinary public
updates retain the installed configuration.

Use [`tools/private_monitor_update.py`](../tools/private_monitor_update.py).
It builds from committed source in an isolated directory under ignored
`dist/private-monitor`, checks the exact runtime hardware ID before applying
the settings, and signs the result with the existing trusted OTA key. The ID
check prevents accidental enrollment of a different unit; it is not encryption.
The binary and its generated private source contain the key and must be shared
privately, never uploaded to GitHub. Normal public release tooling rejects the
private enrollment marker. It is not a way to deliver private credentials in a
public automatic update.

An existing monitoring configuration, including Off, is preserved. A non-secret
`bkmap_seed/enroll` marker records that the private seed has been handled. It
survives firmware-default resets so the same private image will not silently
restore credentials after the owner removes them. The credentials themselves
remain only in `bkmap_mon`, which the reset erases.

## Failure behavior and limits

Monitoring runs in a separate low-priority worker with bounded buffers,
backoff and no subscriptions. It waits for Wi-Fi and synchronized time, avoids
starting TLS when memory is low, and relinquishes TLS during firmware updates.
It stops reporting if the application stops providing fresh snapshots, rather
than repeatedly announcing an old healthy state.

A network outage prevents immediate reporting of that outage. Loss counts are
included after reconnection; the receiver detects silence using last-seen age.
Counts/history are not persisted across power loss. This first version does not
measure historical uptime percentage or diagnose the physical cause of a reset.
The operator can distinguish a reboot using the boot ID, uptime and reset reason.

The optional read-only collector/dashboard lives in
[`tools/telemetry`](../tools/telemetry/). See
[deployment templates](PHASE4AA_DEPLOYMENT.md). No dashboard is automatically
installed by the firmware or enrollment tool.
