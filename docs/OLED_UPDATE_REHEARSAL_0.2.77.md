# OLED automatic-update rehearsal

Both stages passed on COM6 on November 15, 2026. COM5 remained on 0.2.50.
The operator left COM6 automatic updates on with a five-minute check interval.
No manual Check, Install or Restart and no USB flash was used.

1. Scheduled 0.2.75 to 0.2.76 installed the OLED feature. Boot-health confirmation,
   Fresh 8/8 feeds and live LEDs were verified. The first update's download
   was controlled by older firmware without the overlay.
2. Before publishing 0.2.77, the operator was prompted and explicitly confirmed
   readiness to watch. Scheduled 0.2.76 to 0.2.77 then showed download percentages,
   verification/restart stages and post-boot checking/completion. The operator
   confirmed the percentage, Do not unplug power and Update complete all
   displayed perfectly.
3. Serial/API evidence captured automatic discovery and activation, changed boot
   ID/version, OLED phase transitions and SDK health confirmation. An initial
   5/8 feed cycle recovered to Fresh 8/8 by 55 seconds; live LEDs resumed.

Local evidence: captures/auto-update-076 and captures/auto-update-077.
The observer required Fresh 8/8 plus boot confirmation and automatic-install
markers before declaring success. These upgrade observations are not a
12-hour stability run or a forced physical rollback test.

0.2.78 adds the approved gentle pre-update tones and a three-second heads-up.
Its hardware/quiet/cleanup behavior is unit-tested. Audible confirmation of
that new cue requires an update initiated after 0.2.78 is installed.
