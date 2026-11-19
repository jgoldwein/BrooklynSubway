# Brooklyn Subway 0.2.101

Quiet hours and startup greeting now has its own card on the Screen tab, alongside Screen brightness and Screen browsing. The card shows the saved schedule in New York time and the current quiet-hours state, with a clear settings button. Its schedule and state refresh automatically.

A newly active matching station notice flashes a warning triangle five times on the main OLED, then leaves it steady beside **!-see pg 8**. Train counts and the clock remain visible. Two low-high warning pairs play once for a new notice window. Quiet hours and startup muting suppress the sound without a later catch-up cue. Repeated polling and saving unchanged station choices do not repeat the warning. Recent notice windows are remembered for the current boot; restarting clears that history. Alerts do not change train-light colors or wake an intentionally off screen.

Validation: web rendering, unsigned schedule visibility and status JSON; alert-window deduplication and flash timing; 86 OLED runtime scenarios including timer rollover; actual buzzer-driver scheduling, mute, cancellation and failure checks; all four firmware build targets. Hardware appearance and sound of this release have not yet been confirmed by the user.
