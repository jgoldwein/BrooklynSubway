# Brooklyn Map 0.2.14

- Add a separate All subway lines RGB diagnostic covering all 18 routes and
  302 map LEDs. Each LED cycles dim red, green and blue (4/255, 250 ms each),
  with a black frame before advancing. Route ordering follows the semantic
  table; within a route LEDs follow physical-map order. Status LEDs are excluded.
- Preserve the full channel test, numbered channel beeps, individual channel
  tests and LED42 test. POWER and Wi-Fi recovery stop the route test. Startup
  checks and semantic mapping validation precede output; GPIO1 ends LOW.
- Lower the ending startup ding-dong another octave to 655/530 Hz. Opening
  notes, durations, duty cycle and DIM mute remain unchanged.

Normal live-map output stays locked. No board, pin or dependency changes.
Physical observation of the new route sequence and lower pitches is pending.

Validation: route/channel fake-hardware tests, startup tests, release safety
tests, and both versioned network firmware builds.
