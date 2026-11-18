# Brooklyn Subway 0.2.99

Adds a steady **! Alert** to the main OLED title (screen 1/9) when fresh alert data includes a notice active now for a selected station or enabled line-wide notices. The saved map title is shortened to make room; the normal train/light counts and clock remain in their positions.

Turn to Station alerts (8/9), or open Advanced > Station alerts, to read the notice. Upcoming notices do not trigger the indicator. It clears when no matching notice is active, alerts are disabled, or current alert data cannot be confirmed. Absence is not an all-clear and the indicator is not an urgency/severity classification.

No flashing, alert sound, train LED changes, display wake-up, new GPIO assignments or changes to saved personalization. Existing setup/update/test/brightness display priorities are retained.

Validation: station-alert freshness/enabled/current/upcoming checks; 84 OLED runtime scenarios, including active/no-current/unavailable/disabled/stale/other-page indicators, coexistence with the sound badge, clearing after a notice ends, and title bounds; all four production/bench build targets. COM6 USB flash hashes, startup health confirmation and OLED initialization passed with saved personalization loaded. Wi-Fi association had not completed during the startup capture, so a live active-notice display was not verified on hardware; indicator behavior was tested in the simulated display scenarios.
