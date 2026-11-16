# Brooklyn Subway 0.2.86

Fixes a setup handoff that could leave all train lights off after returning to
firmware defaults and connecting to Wi-Fi. Feeds could already be fresh while
physical output remained disabled.

Leaving setup now requests live display startup again. The map still waits for
usable fresh data. POWER-off selection and quiet hours are respected, and an
unhealthy system cannot automatically start output. GPIO assignments, LED power
gate behavior, brightness and current limits are unchanged.

Validation: regression tests cover setup-to-LAN transition before feeds arrive,
automatic startup after fresh data, POWER off, quiet hours and unhealthy exit.
Existing live display tests, 3630 output safety checks and 439 provisioning
checks pass. Production and bench network builds pass. The complete physical
reset-and-reprovision workflow still needs confirmation with this repair.

Installing the signed update preserves settings. Use **brooklyn-s3-network**
for the production PCB and keep power connected throughout installation.
