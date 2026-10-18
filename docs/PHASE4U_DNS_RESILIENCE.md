# Phase 4U: Optional Manual DNS Resolver

## Outcome

Phase 4U adds an explicit, reversible manual DNS resolver setting for the
existing station connection. DHCP remains the default. The setting is disabled
unless enabled by the user in the LAN UI; the firmware never falls back to a
public resolver automatically.

The initial read-only direct-DNS gate passed: UDP/53 queries to both `1.1.1.1`
and `8.8.8.8` received valid RCODE-0 answers for `pool.ntp.org` and
`api-endpoint.mta.info`. These probes did not change the system resolver.

## Behavior and persistence

- The LAN page provides a manual-DNS enable checkbox and an IPv4 address field.
  `POST /config/dns` accepts only the form marker, the optional enabled field,
  and one server address. It rejects malformed addresses, `0/8`, loopback,
  multicast/class-E ranges, and the limited-broadcast address.
- The `bkmap_cfg` NVS namespace stores this setting in one `dns_ovr` blob:
  one enable byte followed by four IPv4 octets in network order. Keeping the
  pair in one key avoids partial enabled/address writes. Missing or invalid
  data safely loads as disabled; existing brightness, route visibility, and
  Wi-Fi credential settings are not cleared or rewritten.
- The resolver is changed only after station association, when DHCP DNS
  addresses have been captured. Enabling changes the primary lwIP resolver;
  disabling restores both captured DHCP resolver slots. On a later
  association, DHCP is captured again before the saved override is reapplied.
- `/api/status` retains `dnsPrimary` / `dnsSecondary` for compatibility and
  explicitly reports `dhcpDns1`, `dhcpDns2`, `manualDnsEnabled`,
  `manualDnsApplied`, `manualDnsServer`, `effectiveDnsSource`,
  `effectiveDnsServer`, and `dnsConfigStatus`. The diagnostics API labels
  system resolver values as `effectiveDns1` / `effectiveDns2`.
- The resolver setting does not change the station IP configuration, Wi-Fi
  credentials, SNTP hostnames, feed URLs, TLS verification, transport timeouts,
  or physical LED output policy.

## Validation

### Host tests and builds

- All 15 host regression runners passed: **1,554,232 checks, 0 failures**.
- Focused LAN/configuration tests: **279 checks, 0 failures**.
- Focused network-diagnostics tests: **112 checks, 0 failures**.
- All four clean PlatformIO builds passed:

  | Environment | RAM | Flash |
  |---|---:|---:|
  | `brooklyn-s3` | 36,268 B | 304,737 B |
  | `xiao-s3` | 35,380 B | 290,053 B |
  | `brooklyn-s3-network` | 88,932 B | 1,086,577 B |
  | `xiao-s3-network` | 88,044 B | 1,069,957 B |

- Both network ELFs had none of the audited physical-output initialization,
  arm/transmit, NeoPixel `show()`, or safety-controller symbols. All four
  PlatformIO environments keep `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`.
- `git diff --check` passed.

### XIAO exercise

The verified XIAO (`94:A9:90:D0:94:54`) was the only target uploaded, on COM4,
using `xiao-s3-network`. The image SHA-256 was
`2DED5E004B31EBED59553BC0A25425837BA9ADAA0FF348AAE787C8E833B8F9D1`. No
production board was uploaded or accessed.

1. Before enabling the override, all four direct UDP/53 checks succeeded:
   each resolver returned RCODE 0 for both hostnames. The DHCP resolver values
   were `0.0.0.0`; normal NTP/MTA DNS checks failed, as expected.
2. Enabling `1.1.1.1` through the LAN form reported `manualDnsApplied=true`,
   `effectiveDnsSource=Manual`, and `effectiveDnsServer=1.1.1.1`.
3. With the override active, system NTP DNS and MTA DNS succeeded, raw TCP/443
   succeeded, SNTP synchronized, and the feed poll completed with **8/8 feeds**.
4. A reboot/reassociation restored the saved `1.1.1.1` override after DHCP;
   status again reported it applied, and the device remained synchronized with
   a complete 8/8 feed cycle. The reboot request's HTTP response timed out
   while the board restarted, but the board rejoined and the post-reboot
   configuration/API checks succeeded.
5. Disabling the override with an empty address restored the captured DHCP
   resolver (`0.0.0.0` on this hotspot) and reported `manualDnsApplied=false`,
   `effectiveDnsSource=DHCP`, and `dnsConfigStatus=disabled`. A multicast
   address submitted to the LAN form returned HTTP 400 and left that disabled
   state unchanged.

Final XIAO state: connected at `192.168.137.25`; manual DNS disabled; physical
LED output disabled; logical brightness 64; route visibility mask 262143 (all
18 routes visible). With the override left disabled and this hotspot still
advertising DHCP DNS `0.0.0.0`, the latest post-upload poll reported `NO FEEDS`
(0/8); the earlier 8/8 poll completed while manual DNS was enabled. Tailscale
and host network settings were not changed.
