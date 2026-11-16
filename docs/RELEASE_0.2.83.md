# Brooklyn Subway 0.2.83

Captive setup at **192.168.4.1** now uses separate steps:

1. Create the administrator password and press **Save password and continue to Wi-Fi**.
2. Select Wi-Fi, enter its password and connect.

Only one editable form appears at a time. If the map already has an
administrator password, setup opens the Wi-Fi step with a separate
**Change the map password** link. Wi-Fi connection cannot proceed until an
administrator password has been saved. This also applies without JavaScript
or when submitting an older setup page.

Installing this update preserves all settings. Firmware defaults remains a
separate, confirmed action under Advanced. Keep power connected during updates.

Validation: 410 provisioning checks, 166 administrator checks and reset recovery
tests pass. Production PCB and XIAO bench network builds pass. The revised
physical captive-portal workflow has not yet been retested on a board.

Use the **brooklyn-s3-network** signed package for the production PCB. Optional
.local discovery remains disabled; use the IP address displayed on the OLED.
