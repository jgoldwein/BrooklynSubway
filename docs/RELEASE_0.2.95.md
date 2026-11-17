# Brooklyn Subway 0.2.95

Listening-test release: adds a Rangers synth-horn easter egg using the supplied F#3, A#3 and C#4 pitches (185/233/277 Hz), with dim red/white/blue map flashes and an OLED GOAL screen. One passive buzzer cycles the notes; it cannot reproduce the simultaneous synthesizer chord. This is a synthesized approximation, not a recording, a verified identical horn, or the Slapshot melody.

Reference supplied by Joel: [All 32 NHL Goal Horns on Synthesizer](https://www.youtube.com/watch?v=DCeMMxCf7kI), by Trainocat. No recording is embedded or distributed.

Hold TEST + ROUTE together for two seconds, release both, and repeat to cycle through the extras. Rangers is sixth, after the anniversary preview. Let each extra finish before repeating; POWER stops it. Color pulses alternate 240 ms on and 240 ms black. The map cap is 3/255 per color channel (white uses 2/255); the existing lower quiet-hours cap and POWER display state take priority. Quiet hours mute sound.

Existing personalization, Wi-Fi and administrator settings are preserved. No new firmware dependency or pin assignment is introduced. Signed OTA packages retain signature/target checks and rollback health confirmation. Existing intermittent feed transport failures remain under investigation.

Validation: all four firmware targets build. Host tests verify chord pitch order, blast boundaries, red/white/blue and black-frame timing, full-map estimated-current limits, existing easter eggs, quiet/sound behavior and physical-output safety. COM6 USB flash hashes and startup health confirmation passed. Human listening and visual feedback on this new effect are pending.
