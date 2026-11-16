# Brooklyn Subway 0.2.82

Adds **Advanced → Firmware defaults** for preparing the map for a new owner.
Administrator sign-in and an explicit confirmation are required. The reset
removes saved Wi-Fi, the administrator password and customized settings, keeps
the installed firmware, and restarts into **BrooklynMap-Avi** setup at
**192.168.4.1**. The new owner creates an administrator password during setup.
Automatic update preferences also reset; enable them again after setup.

Also restores requested live output after a temporary loss of fresh data and
adds rate-limited Wi-Fi reassociation after repeated failed download cycles.
POWER, quiet hours and hardware safety limits remain in effect.

Production and bench builds pass. Automated tests cover reset interruption and
retry, authentication, confirmation rendering, provisioning, output safety and
display/network recovery. The complete erase-and-reprovision workflow has not
yet been exercised on a physical board. Earlier production OTA updates have
been tested; this release does not claim a completed extended stability run.

Use the **brooklyn-s3-network** package for the production PCB. The XIAO package
is a bench target. Updates require a valid signature and the matching target.
Keep power connected throughout installation. Installing the update preserves
settings; settings are erased only when the reset action is confirmed.

Optional .local discovery remains disabled; use the IP address on the OLED.
