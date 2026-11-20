# 0.2.104 - Network addresses in MQTT reports

Status: prepared next release; publication waits for confirmation that the
shipped map has installed 0.2.103. No embedded individual credentials are needed
in this ordinary update; existing monitoring settings are retained.

- Enabled monitoring includes LAN IPv4 and cached public WAN IPv4, with `null`
  for unknown/expired addresses. Existing topics, credentials and v1 schema remain.
- WAN lookup uses verified HTTPS to ipify, with a 64-byte response limit and
  strict public IPv4 parsing. No lookup occurs until monitoring is provisioned
  and On. Refresh about hourly; failures retry after five minutes.
- The shared auxiliary worker runs the lookup between MTA feed cycles. MQTT
  releases TLS briefly during lookup to constrain concurrent heap usage. No new
  task, third-party library, GPIO or hardware change.
- Connection loss or monitoring Off invalidates the WAN cache; results from an
  obsolete session are discarded. An expired or failed lookup never prevents
  ordinary heartbeat reports.
- Address fields replace two optional internal statistics (`x_ev_dropped`,
  `x_reconnects`) to preserve the eight-extension limit. Wi-Fi/MQTT loss counts,
  last-loss uptimes, missed acknowledgments and publish failures remain.
- Addresses are diagnostic observations, not device authentication or remote
  web access. WAN lookup may see router/VPN/carrier-NAT egress.

Validation: WAN parser/cache 77 checks; telemetry core 268 checks and 30 frozen
schema checks; monitoring configuration/privacy 83 checks; web policy 1035
checks; existing collector/store/hardening/deployment suites passed. All four
production/bench build targets passed. Device testing of the new lookup remains
pending. No board upload or publication yet.
