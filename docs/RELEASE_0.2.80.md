# Brooklyn Subway 0.2.80

- Two gentle ascending notes confirm successful settings saves, including web
  controls, Wi-Fi/password setup, update preferences and physical brightness.
  Nonblocking, rapid saves coalesce, quiet hours mute, failed saves stay silent.
- Retains quiet-hours resume intent if data is temporarily unavailable at the
  scheduled end. Fresh data can then resume the map automatically.
- Large TLS allocations prefer the production board's PSRAM; certificate and
  package-signature verification remain enabled.
- Disables optional .local discovery after a captured SDK crash in
  _mdns_remove_scheduled_answer / mdns_parse_packet. Use the board's IP address
  shown on its OLED/device page; ordinary DNS and GitHub updates still work.

The short real quiet-hours transition test passed on private 0.2.79. Its TLS
allocation failures recovered, but the extended run captured a separate mDNS
panic. That is why this release includes the discovery-service workaround.
The new code requires a fresh stability run; no uninterrupted 12-hour success
is claimed for the interrupted 0.2.79 observation.

No pin, signing-key, dependency or brightness-cap changes. Production MCU remains
ESP32-S3-WROOM-1U-N16R8. The XIAO package remains a bench/toolchain target.
