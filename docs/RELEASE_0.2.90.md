# Brooklyn Subway 0.2.90

Makes startup readiness sounds distinctive and stops repeated green-status
beeps: one low SYSTEM beep, two middle Wi-Fi beeps and three high MTA beeps,
each only once per service per boot.

Adds optional service alerts under **Screen → Quiet hours and startup greeting**.
They are off by default. Three descending notes announce a continuous 60-second
loss; three ascending notes announce restoration only if loss was audibly
announced. There is no repeated alarm during one outage. Startup, setup, quiet
hours and short interruptions do not generate delayed catch-up alerts. Wi-Fi
disconnection or train data older than 90 seconds begins the loss timer;
usable partial updates do not count as an outage just because a feed is missing.
Existing saved-setting and update-start patterns remain distinct two-note cues.

Overview's **Train data, colors & beep tones** section now identifies the latest
sound and its age, refreshing about every two seconds. It reports actual cue
starts, not muted requests. The public status API includes `lastSound` and
`serviceSoundsEnabled`. No passwords or private sound recordings are involved.

Audio remains on GPIO21 using the existing shared PWM channel and input task;
there are no added tasks or dependencies. Reset defaults leaves service alerts
off. Quiet-hour settings, firmware signatures and LED power limits remain intact.

Validation covers precise pulse counts, service-loss timing and rollover,
acknowledged restoration, no repeated outage alarms, opt-out/quiet cancellation,
PWM failure cleanup, persistence, form validation, output safety, Overview and
status rendering, and existing OLED runtime behavior. Physical recognition of
the new patterns still needs user confirmation after upgrading.

Keep power connected during the signed update. Installing preserves settings.
