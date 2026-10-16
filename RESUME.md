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

## Current checkpoint — Phase 4M checkpoint / Phase 4N next, 2026-10-16

- **Baseline:** Phase 4L commit `333682eaff40cf4a2e0b845fedb3d54a01c1c908`,
  tag `phase-4l-wifi-provisioning`. Phase 4M is validated and ready for its
  requested commit/tag/push; update this checkpoint with the resulting hash
  before beginning Phase 4N.
- **Phase 4M worktree:** implementation adds `include/operational_display.h`,
  `src/operational_display.cpp`, `tools/host_tests/operational_display_test.cpp`,
  `tools/run_operational_display_tests.py`, and modifies `main.cpp`,
  `network_service.h/.cpp`, and this file; it adds
  `docs/PHASE4M_OPERATIONAL_DISPLAY.md`. Review live `git status` for
  authoritative file state.
- **Policy:** fixed-size controller retains the committed `TrainFrame`;
  `kPartialGraceMs=90000`, `kHardStaleLimitMs=300000`. It only sees a candidate
  after a full transport cycle. Recent COMPLETE + PARTIAL holds old frame;
  prolonged usable PARTIAL commits; NO FEEDS holds until the hard limit;
  hard stale blanks and later usable data recovers. Freshness uses wrap-safe
  monotonic milliseconds, never SNTP.
- **Status / safety:** RGB status overlay is memory-only at canonical status
  identities; map colors remain in the existing renderer. Controller is 344 B;
  committed RGB frame adds 918 B. Physical backend initialization was removed
  from main startup for this memory-only phase; backend code and LED mapping
  are unchanged. `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`; no upload. Production
  PCB is untested. XIAO serial monitor was closed at the Phase 4L checkpoint;
  this phase does not require connecting or uploading hardware.
- **Validation completed:** semantic tables passed (302 LEDs, 276 STATION,
  26 SEGMENT, 188 registry entities); resolver 958,614, GTFS adapter 7,383,
  GTFS-RT 429, previous stop 8,098, TrainFrame 5,232, RGB renderer 71,785,
  TrainPipeline 4,538, feed transport 1,209, Wi-Fi provisioning 310, and
  operational display 1,534 checks all passed with zero failures. All four
  builds passed: brooklyn-s3 35,308 B RAM / 303,629 B flash; xiao-s3
  34,428 B / 288,673 B; brooklyn-s3-network 70,812 B / 1,023,889 B;
  xiao-s3-network 69,932 B / 1,008,957 B. Final safety/scope and
  `git diff --check` review passed. No protected mapping, transport-policy,
  manufacturing, or secrets files changed; the ignored local
  `include/secrets.h` remains untracked.
- **Next action:** checkpoint Phase 4M with commit
  `Phase 4M: add operational display state management`, annotated tag
  `phase-4m-operational-display`, and push `main` plus the tag. Then begin
  Phase 4N as uncommitted, memory-only work. Never upload firmware.
- **Prior hardware/security context:** Phase 4L XIAO provisioning, NVS
  persistence, W recovery, dynamic success-page links, and post-reboot feed
  cycles were verified. The wildcard arbitrary-unknown-URL physical browser
  test remains a minor caveat. Never write passwords or other credentials in
  this file; local `include/secrets.h` is ignored, untracked, and must not be
  staged.
