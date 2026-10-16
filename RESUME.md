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

## Current checkpoint — Phase 4N pushed; Phase 4O docs ready, 2026-10-16

- **Phase 4N checkpoint:** commit
  `ce80187b31267caa2a7006151cb3188c8cd3de46`, tag
  `phase-4n-output-bridge`, pushed to `origin/main` and `origin`.
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
  disconnected and stops before physical-LED testing. This documentation is
  uncommitted and separate from Phase 4N.
- **Validation:** semantic data (302 map LEDs, 276 STATION, 26 SEGMENT, 188
  registry entities) and complete host regression passed: resolver 958,614;
  GTFS adapter 7,383; GTFS-RT 429; previous stop 8,098; TrainFrame 5,232;
  RGB renderer 71,785; TrainPipeline 4,538; feed transport 1,209; Wi-Fi
  provisioning 310; OperationalDisplay 1,534; output bridge 476,271 checks.
  All had zero failures. Four clean builds passed: brooklyn-s3 36,236 B RAM /
  304,353 B flash; xiao-s3 35,348 B / 289,453 B;
  brooklyn-s3-network 71,740 B / 1,024,605 B;
  xiao-s3-network 70,852 B / 1,009,733 B.
- **Safety:** every PlatformIO environment keeps
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. No firmware upload or physical test.
  The production PCB has not been made available for this validation and
  remains physically unvalidated. External 5 V and physical LEDs remain
  prohibited until separately authorized.
- **Next safe hardware action:** when the production PCB is available, perform
  the checklist's USB-only bring-up with external 5-V/barrel power
  disconnected. Stop before any physical-LED action.
- **Next repository action:** complete the Phase 4O documentation-only
  checkpoint separately. Never record credentials.
