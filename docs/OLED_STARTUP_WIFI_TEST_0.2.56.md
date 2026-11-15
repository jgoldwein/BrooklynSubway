# Private COM6 build 0.2.56 - startup greeting waits for Wi-Fi

Startup greeting can now remain until Wi-Fi connects and has a nonzero LAN IP,
with a five-second minimum for readability. It does not wait for NTP or feeds.
Normal idle return is suspended during that startup wait. Turning skips it;
setup/POWER keep priority. Later Wi-Fi loss does not restart the greeting.

Screen > Quiet hours and startup greeting has a separate Keep it until Wi-Fi
connects checkbox. This defaults on. Unchecking restores the brief five-second
option; unchecking Show dedication disables the startup greeting. Existing 0.2.55
preferences retain quiet-hour fields and adopt Wi-Fi wait by default. A schema bit
lets an explicitly saved brief choice remain brief on subsequent boots. No NVS
write occurs merely to migrate a legacy loaded preference. All preferences still
use one atomic 64-bit screen_opts save; unknown upper bits remain invalid.

User confirmed COM6 was ready after OLED-case work and requested centering of
all dedication items except scrolling names. First four lines are now centered
using actual font widths; scrolling names remain left aligned. Current wording
Happy Bar Mitsvah! / Avi, Goldweins message, date and names are retained. MODE
remains reserved. Main frame and quiet LED/audio policies are unchanged.

All 31 suites, four builds and browser four-width tests pass. Tests cover early
Wi-Fi / five-second minimum, delayed Wi-Fi beyond idle timeout, one-shot startup,
turn/setup/POWER overrides, actual rendered text centering, legacy preference
migration, explicit brief persistence and the checked native form field. Static
production RAM remains 198316 B; largest escaped home HTML remains 24336 B.

Frozen dist/0.2.56/factory flashed to serial-verified COM6
28:84:85:34:1F:74 at 115200; all hashes verified and hard reset completed. Serial
confirmed OLED ACK/first frame at 0x3C, Wi-Fi-wait greeting, return to Main and
firmware health confirmation. Initial G feed timed out (HTTP -11, no TLS error,
0 bytes, 9387 ms); retain as a network observation rather than claim 8/8 startup.
COM5 master remains 0.2.50, read-only checked Fresh 8/8 afterward. No stable
public release/tag; operator still needs to confirm physical appearance.
