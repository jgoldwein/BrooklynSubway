# Brooklyn Subway 0.2.84

On a boot that starts in Wi-Fi setup, the OLED now shows the existing Bar
Mitzvah greeting for three seconds, then automatically displays the
**BrooklynMap-Avi** network and **192.168.4.1** setup instructions.

Later entry into setup using the 10-second knob hold shows the instructions
immediately. The startup greeting preference is respected; turning it off
skips this greeting. Waiting for Wi-Fi does not extend the setup greeting.
Firmware-update warnings retain priority. OLED wiring and driver are unchanged.

Validation: all 66 OLED runtime scenarios pass, including the three-second
boundary, timer rollover and upgrade priority. Existing navigation and clock
tests pass. Production and bench network builds pass. The physical OLED
appearance has not yet been checked on a board.

Installing this signed update preserves settings. Keep power connected during
installation. Use the **brooklyn-s3-network** package for the production PCB.
