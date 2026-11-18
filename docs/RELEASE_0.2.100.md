# Brooklyn Subway 0.2.100

The main OLED alert prompt now reads **! Alert: see page 8**, making the next step clear. Turn the knob to Station alerts (8/9) for the short notice summary, or open Advanced > Station alerts for longer details.

The prompt replaces the main title only while a confirmed current notice is active. Train/light counts and the clock remain in place. Upcoming-only, disabled, stale and unavailable alert data do not trigger it. No flashing, new sound, map-light change or wake-up of a powered-off display.

Validation: exact prompt text and 128-pixel display bounds in the existing 84 OLED runtime scenarios; current/upcoming/freshness alert checks; all four production/bench build targets. COM6 flash hashes, OLED initialization and startup health confirmation were checked; live alert text was verified in display simulations. Wi-Fi association had not completed during the startup check, so a live active MTA notice was not verified on hardware.
