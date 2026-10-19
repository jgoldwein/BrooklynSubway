# Superseded: Tailscale subnet-router remote-access plan

**Status: SUPERSEDED / OPTIONAL EXPERIMENT ONLY.**

This document is retained as historical engineering context. Its deployment
recommendations and validation steps are not current product requirements or
approved work.

The Brooklyn Map product no longer requires:

- a native ESP32 Tailscale client;
- a Tailscale subnet router;
- Tailscale remote administration or VPN-based monitoring; or
- inbound remote access of any kind.

The preferred future architecture is **user-opt-in, device-initiated outbound
operational telemetry plus separately authenticated and signed OTA/update
mechanisms**. It requires no inbound port and has no Tailscale dependency.
Telemetry is monitoring-only initially; it is not a remote administration
channel. Signed OTA remains separately authenticated and is not initiated by
remote telemetry commands.

The notes below describe the earlier Tailscale readiness audit only. Do not
follow its setup commands or deployment checklist as current product direction.

## Historical decision snapshot (superseded)

No Tailscale client runs on the ESP32. The map stays an ordinary LAN device and
an existing always-on LAN host advertises the map's subnet into the tailnet.
**No Brooklyn firmware change is required.**

```
remote Tailscale client
  -> encrypted Tailscale tunnel
  -> subnet router on the map's LAN
  -> plain HTTP on the local LAN
  -> Brooklyn map (port 80)
```

The tunnel is encrypted; the final subnet-router-to-map hop is ordinary,
unencrypted HTTP on the local LAN. This is not end-to-end HTTPS and must not be
described as such.

## Historical bench observations (2026-10-18, temporary hotspot)

- XIAO: `192.168.137.191`, mask `255.255.255.0`, gateway `192.168.137.1`
  (the Windows Mobile Hotspot), so subnet `192.168.137.0/24`. The address
  changed from earlier sessions (`.25`, `.182`), which shows why a DHCP
  reservation is needed.
- This is a temporary bench subnet, not the production-home subnet. Do not
  advertise it.
- Windows Tailscale: service running (automatic start) but the client reports
  "stopped" (disabled by the user); tailnet address `100.98.119.76` is shown
  by the CLI but the client is not connected. Advertised routes could not be
  read while stopped. The Tailscale adapter has IP forwarding disabled and
  the global `IPEnableRouter` is 0 (the hotspot adapters have forwarding
  enabled by Internet Connection Sharing). The hotspot subnet is therefore not
  routed into any tailnet.

## Former deployment recommendation (not current guidance)

- Prefer an existing always-on LAN host as the subnet router (for example an
  existing Raspberry Pi, Linux server, or a Home Assistant Tailscale node)
  rather than adding a new one. Do not use the Windows PC.
- It must be on the same LAN/VLAN as the map and able to reach the map on
  TCP/80. Client or AP isolation must not block it.
- Advertise the map's real LAN subnet (or just the map's `/32`).

Linux subnet router, for the eventual deployment (replace the subnet):

```
sudo sysctl -w net.ipv4.ip_forward=1    # persist via /etc/sysctl.d
sudo tailscale up --advertise-routes=<LAN-CIDR>
```

Then approve the route in the Tailscale admin console and restrict it with an
ACL/grant so that only the intended users or tags reach `<map-ip>:80`.

## Address stability

1. DHCP reservation on the LAN router: preferred.
2. A stable local DNS name where the routed DNS supports it.
3. Hard-coded static IP in firmware: avoid; no static-IP configuration is added.

## mDNS limitation

`brooklynmap.local` works on the local LAN only. mDNS does not normally cross a
routed Tailscale subnet. Remote access should initially use
`http://<stable-LAN-IP>/` unless a proper DNS or MagicDNS record is created
separately. The firmware's mDNS is not modified for this purpose.

## Phase 4V authentication stays in force

Tailscale access does not bypass Phase 4V and no "trusted Tailscale" bypass
may be added:

- `GET /`, `/api/status` and `/api/network-diagnostics` remain public and
  read-only for anyone who can reach the map.
- Brightness, routes, DNS, diagnostics runs, Wi-Fi change, reboot and password
  change still require administrator login, a session cookie and a CSRF token.
- First-time password setup and reset remain available only on the local
  `BrooklynMap-Avi` setup AP.
- The login and session cookie cross the local hop in clear HTTP, so only use
  this on a LAN you trust, and prefer limiting the route with ACLs.

## Archived validation checklist (not current guidance)

1. Confirm the production-home subnet and give the map a DHCP reservation.
2. Confirm which host will be the subnet router; check its IP forwarding.
3. Advertise only the required subnet or `/32` and approve it in the console.
4. Add an ACL/grant limiting access to `<map-ip>:80`.
5. From a remote Tailscale client, open `http://<map-ip>/` and confirm the
   read-only page and `/api/status`.
6. Confirm an anonymous remote POST to `/config/brightness` returns 401.
7. Log in remotely and make a harmless change (brightness 64 to 65 and back).
8. Confirm logout and that the map does not become reachable from outside the
   tailnet.
9. Record the result and any ACL decisions here.
