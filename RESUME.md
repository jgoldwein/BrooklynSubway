# Resuming work after a reboot

Use this file when returning after a planned or unexpected computer, VS Code, or
agent-session restart. The checkpoint below is a handoff, not a substitute for
checking the live repository and hardware state.

## Resume procedure

1. Open the repository root and read `AGENTS.md` and `PROJECT_CONTEXT.md` before
   changing firmware. Then read this file and the task-specific document linked
   in the current checkpoint.
2. Inspect `git status`, the current branch and latest commit, and review
   `git diff` (including untracked files). Treat every existing change as user
   work: preserve it, do not reset, check out, clean, or overwrite it. Compare
   the live state with the checkpoint below; the repository is authoritative if
   they differ.
3. Check which board is actually connected and whether it is powered before
   uploading, resetting, or running hardware tests. The XIAO is a bench target,
   not a substitute for the manufactured PCB. Never flash the production target
   based on an assumption. Ask Joel if the target or safe next step is unclear.
4. Continue from the checkpoint's next actions. Re-run only the targeted
   validation needed for the change, and report any test that could not be run.
5. Before ending a work session or intentionally rebooting, update the
   checkpoint below: record the date, task/status, relevant commit, changed
   files, validation results, hardware/firmware state, and the next concrete
   action. Do not record passwords, tokens, or other secrets. Keep the history
   concise and replace stale status rather than accumulating contradictory
   instructions.

If the chat history is unavailable, do not guess what the user intended from
the changed files alone. Use this checkpoint and the git history; ask Joel for
clarification only if the requested next action remains ambiguous.

## Current checkpoint — Phase 4Q safe output arming checkpointed

- **Phase 4P baseline:** commit `da3195f`, tag
  `phase-4p-physical-output-audit`.
- **Phase 4Q checkpoint:** tag `phase-4q-safe-output-arming` is the recovery
  reference; verify it resolves to HEAD before any future work. Phase 4Q is
  the latest firmware phase. Do not begin another firmware phase.
- **Phase 4N result:** memory-only bridge accepts a completed 306-pixel RGB
  frame, validates all canonical map round trips and the 302-map / 4-status
  split, stages it, and publishes through the LED engine's inactive buffer
  followed by one active-pointer swap. Atomicity is limited to the synchronous
  main-loop model; there is no asynchronous-consumer guarantee. The bridge
  does not render colors, rescale brightness, initialize or transmit physical
  LEDs, call `show()`, or touch LED_ENABLE.
- **Brightness / power:** logical brightness remains in the RGB renderer at
  64/255; LED engine brightness remains 255/255. Existing all-306-pixel,
  20 mA/component estimate, global scale, 4,000 mA budget, and production cap
  16/255 are unchanged and have not been validated against measured PCB
  current.
- **Phase 4O readiness:** `docs/PRODUCTION_PCB_BRINGUP_CHECKLIST.md` was
  reviewed against the requested staged sequence and stop conditions. It
  clearly limits the initial stage to USB-only with external 5-V/barrel power
  disconnected and stops before physical-LED testing. It states that LED
  output, external LED power, and LED_ENABLE are not authorized; all builds
  must retain `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. Existing current/power
  policy is not validated on real production hardware.
- **Validation:** semantic data (302 map LEDs, 276 STATION, 26 SEGMENT, 188
  registry entities) and complete host regression passed: resolver 958,614;
  GTFS adapter 7,383; GTFS-RT 429; previous stop 8,098; TrainFrame 5,232;
  RGB renderer 71,785; TrainPipeline 4,538; feed transport 1,209; Wi-Fi
  provisioning 310; OperationalDisplay 1,534; output bridge 476,271 checks.
  All had zero failures. Phase 4N baseline clean builds: brooklyn-s3 36,228 B
  RAM / 304,181 B flash; xiao-s3 35,340 B / 289,281 B;
  brooklyn-s3-network 71,732 B / 1,024,453 B;
  xiao-s3-network 70,844 B / 1,009,581 B.
- **Safety:** every PlatformIO environment keeps
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. No firmware upload or physical test.
  The production PCB has not been made available for this validation and
  remains physically unvalidated. External 5 V and physical LEDs remain
  prohibited until separately authorized.
- **Phase 4P audit:** `docs/PHASE4P_PHYSICAL_OUTPUT_AUDIT.md` documents the
  read-only audit. It identified that powered WS2812-compatible LEDs may
  retain their last latched color through MCU reset; LED_ENABLE LOW alone does
  not clear them.
- **Phase 4Q implementation:** `docs/PHASE4Q_SAFE_OUTPUT_ARMING.md` records
  the black-first state controller, backend integration, validation, and
  limitations. The invariant is: no non-black physical frame can be
  transmitted unless a successful post-enable all-black transmission has
  completed during the current arming session. The ordinary `main.cpp` path
  remains memory-only.
- **Phase 4Q files:** `include/physical_output_safety.h`,
  `src/physical_output_safety.cpp`, `src/led_backend.cpp`, `src/led_power.cpp`,
  `tools/host_tests/physical_output_safety_test.cpp`,
  `tools/run_physical_output_safety_tests.py`, the Phase 4Q document, and
  this handoff.
- **Validation:** all twelve Phase 4 host regression runners passed. The
  black-first safety runner passed 3,630 checks using the production limiter;
  final checks cover color-transmission failure and fresh-black re-arming.
  Semantic validation confirmed 302 map LEDs, 276 STATION, 26 SEGMENT, and
  188 registry entities. Clean-build sizes, Phase 4N deltas, and all
  regression counts are in `docs/PHASE4Q_SAFE_OUTPUT_ARMING.md`. The
  production zero-permission ELF has no linked physical renderer,
  initializer, arming, transmission, interlock, strip-construction, or
  NeoPixel `show()` symbols.
- **Safety:** all environments retain
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. No upload, physical LED write, or
  physical test occurred. The production PCB remains untested. Phase 4Q is
  checkpointed to `origin/main` with tag `phase-4q-safe-output-arming`.
- **Next safe action:** WAIT FOR THE PRODUCTION PCB, then perform the
  documented USB-only Phase 4O bring-up with external 5-V/barrel supply
  disconnected. Do not enable physical LEDs or begin another firmware phase.
  Never record credentials.

## Recovery when the manufactured PCB is available

1. Read this file and `docs/PRODUCTION_PCB_BRINGUP_CHECKLIST.md`.
2. Verify the current tag is `phase-4q-safe-output-arming`.
3. Keep external 5-V/barrel supply disconnected; connect USB only.
4. Keep physical LED output disabled.
5. Perform the USB-only checklist in order.
6. Stop and report results before any external LED-power step.
