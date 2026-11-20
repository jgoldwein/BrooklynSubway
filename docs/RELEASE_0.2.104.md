# 0.2.104 - LED transmission hardening and network addresses in MQTT reports

Status: prepared next release; publication waits for confirmation that the
shipped map has installed 0.2.103. No embedded individual credentials are needed
in this ordinary update; existing monitoring settings are retained.

- Production LED output uses one persistent ESP-IDF 4.4 RMT transmitter on
  core 1, four hardware memory blocks, and an IRAM level-3 interrupt handler.
  Each channel's complete GRB pulse sequence is prepared in internal RAM before
  transmission. This replaces the per-channel NeoPixel driver installation and
  interrupt-time byte translation. Pin assignments and color/power limits remain.
- All RMT configuration, routing, start and completion results are checked.
  A reported transport error or 100 ms completion timeout forces GPIO1 LOW;
  a faulted transmitter requires a reboot. A startup serial line identifies
  the driver, memory allocation and timing. The COM6 short visual trial showed
  no flashes with monitoring Off or On; longer-term observation remains useful.

- Enabled monitoring includes LAN IPv4 and cached public WAN IPv4, with `null`
  for unknown/expired addresses. Existing topics, credentials and v1 schema remain.
- WAN lookup uses verified HTTPS to ipify, with a 64-byte response limit and
  strict public IPv4 parsing. No lookup occurs until monitoring is provisioned
  and On. Refresh about hourly; failures retry after five minutes.
- The shared auxiliary worker runs the lookup between MTA feed cycles. MQTT
  releases TLS briefly during lookup to constrain concurrent heap usage. No new
  task or third-party library for address lookup.
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
production/bench build targets passed for the address change. COM6 MQTT reports
now confirm nonempty LAN and WAN fields within the eight-extension limit.
LED transport tests cover all 306 RGB pixels across all eight channels,
repeat initialization/transmission, GRB pulse timing/reset, invalid input and
injected core/configuration/install/clock/routing/start/completion failures.
Existing diagnostic, live-display and physical output safety tests pass.
Production network, diagnostic PCB and network bench builds pass after the LED
change. COM6 reports Fresh 8/8 feeds with monitoring enabled, no reported
Wi-Fi/MQTT disconnects, and the user observed no transient flashes in the short
trial. This is not a long stability test or an electrical waveform measurement.
This release has not been published.
