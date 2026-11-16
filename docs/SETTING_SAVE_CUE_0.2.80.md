# Saved-setting cue — queued 0.2.80

The user subsequently requested publication, authorizing an intentional restart
of the running COM6 recovery test. Before publication, a captured panic was
decoded to the SDK mDNS discovery service; 0.2.80 also disables that optional
service. Direct IP access and ordinary DNS/TLS remain available.

A successful user setting save requests two gentle ascending notes: 1200 then
1800 Hz, 60 ms each, separated by 50 ms at duty 64/255 on the existing GPIO21
buzzer channel7/timer3. The existing input-task audio service runs the cue
without delay calls. One pending event and a 250 ms settling interval coalesce
rapid writes; there is no unbounded sound queue. Quiet hours suppress and cancel
it without delayed catch-up. Diagnostics/recovery and higher-priority effects
can cancel queued feedback. Failed PWM setup leaves the buzzer LOW.

Success-only call sites:

- Main web page: OLED address, train colors, uncertain-location settings,
  screen/quiet/startup preferences, screen return timeout, OLED brightness,
  MTA poll interval, map brightness, DNS and visible subway routes.
- Administrator password changes and setup-network initial/reset password.
- Update check interval and automatic-update preference.
- Newly connected Wi-Fi credentials, only after successful NVS commit.
- Physical brightness changes that actually changed and saved a value.

Invalid requests, failed persistence, brightness limits, sign-in/sign-out,
ordinary navigation, train-feed refreshes and internal runtime checkpoints do
not request the saved cue. Existing level-pitched brightness feedback remains;
its timer cleanup preserves the pending success cue.

The screen-settings handler applies the new quiet policy before requesting its
cue. Saving a schedule that makes quiet hours active is therefore silent.
No new NVS keys, tasks, libraries, pins or web-page buffer growth.

Tests cover pitches, duty/duration/gap, nonblocking service, coalescing, quiet
cancellation, maintenance suppression, either-note PWM failure, timer rollover,
and preservation across brightness feedback. Real acoustic/save-flow validation
is pending installation after the current recovery test.
