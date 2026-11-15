# OLED and MODE test build 0.2.51

Private COM6 test build; not published as a stable update. COM5 master remains
on 0.2.50 for the scheduled stability capture.

Press and release MODE to cycle:

1. Main: Brooklyn Subway, tracked trains / lit map LEDs, New York clock.
2. Train activity: counts of visible approaching, stopped, leaving and uncertain
   map lights. These are LED indications, not counts of distinct trains.
3. Connection: Wi-Fi signal in dBm, last completed cycle feeds out of all eight,
   display freshness and age of the last usable update.
4. Device: firmware version, individual LAN IP and uptime; next press returns Main.

MODE indicator LED307 (channel 8, zero-based index 3, data GPIO13) follows the
selected screen: Main off, Activity cyan, Connection green, Device purple.
It is visible only when map LEDs are enabled and follows the existing brightness
and current caps. The existing logical white setup indicator is preserved;
recovery still disables physical LED output. MODE is GPIO41, active LOW.
Selection resets to Main at boot and does not write flash settings.

OLED uses existing U22 SDA GPIO8 / SCL GPIO9 and its proven driver. Wi-Fi setup
instructions take priority and MODE cannot dismiss them. Brightness feedback
keeps its temporary priority. MODE does not wake a POWER-disabled OLED. The
four-step POWER sequence, 3-second POWER reboot and 10-second rotary recovery
are unchanged. No train-position, palette, polling, route-selection or TEST
sequence change is included in this build.

Implementation: existing release-debounced button policy samples MODE in the
20 ms live task, including during feed fetches. Held-at-boot and diagnostic-held
releases do not advance screens. Selected screen is atomic. The main loop
publishes a small diagnostic snapshot once per second; a short critical section
only copies it, and the OLED worker alone accesses I2C. The display ages the
snapshot using wrap-safe uptime without accessing Wi-Fi or the train pipeline
from the OLED task. Five bounded 6x12 text lines fit within 128x64.

Validation: all 30 host suites and all four firmware targets pass. Tests cover
all screen text bounds, absent Wi-Fi/data, freshness age and uptime rollover,
setup priority, OLED-off behavior, and MODE status changes without changing map
pixels. Existing POWER/recovery/current-limit tests still pass. Production
static RAM is 197900 bytes (+88). No dependency or pin-assignment change.

Flashed the frozen 0.2.51 bundle to COM6 after verifying USB serial
28:84:85:34:1F:74. Esptool hashes verified. Serial startup confirms OLED first
frame at 0x3C, 8/8 feeds, live output and OTA health confirmation; logs are under
captures/*051-COM6.txt. Physical MODE presses / screen appearance need operator
confirmation. Do not publish this as latest stable during the master's 0.2.50
stability run without deliberately coordinating that change.
