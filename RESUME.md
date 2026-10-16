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

## Current checkpoint — Phase 4L, 2026-10-16

- **Git:** Phase 4L checkpoint is based on `44fe102`
  (`Portal: auto-refresh connecting page, clearer saved message, longer AP
  linger`), a legitimate descendant of Phase 4K `c85f083`
  (`phase-4k-hardware-network-bringup`). Resolve the exact checkpoint commit
  with `git log -1 --oneline --decorate`; the Phase 4L tag is
  `phase-4l-wifi-provisioning`. No history rewrite.
- **Hardware state:** the latest XIAO network-enabled wildcard-handler
  firmware was flashed to the XIAO on COM4 and verified by hash. The serial
  monitor is closed. No firmware was uploaded during final checkpointing.
  Production PCB is not physically tested and must not be flashed without
  approval.
- **Verified hardware flow:** stored NVS credentials survived firmware
  upload/reboot and booted normally. `W` opens recovery without erasing
  credentials. `BrooklynMap-Avi` starts at `192.168.4.1`; phone joined and scan
  found networks. Credentials were submitted without exposing the password.
  The XIAO joined the selected LAN and obtained DHCP IP `10.110.202.133` for
  that run; credentials saved, AP lingered for 8 seconds then stopped, SNTP
  synchronized and MTA polling resumed. Three subsequent cycles completed
  8/8. User visually confirmed the success page showed joined SSID, clickable
  LAN-IP link, clickable setup link and AP-close note. After reboot without
  `W`, serial capture showed no setup messages, repeated `LED outputs
  DISABLED`, and two normal COMPLETE 8/8 cycles. USB reset interrupted
  startup output, so NVS source and SNTP lines were not captured in that last
  reset retest; both normal stored-credential operation and SNTP had been
  verified in earlier hardware logs.
- **Captive-path fix:** Arduino `WebServer` logs "request handler not found"
  before calling `onNotFound`. A wildcard `HTTP_ANY` handler registered after
  `/`, `/scan` and `POST /connect` redirects unmatched requests to
  `http://192.168.4.1/`; common Android/iOS/Windows probes still receive the
  portal HTML. The wildcard was host-tested, built and flashed to the XIAO.
  Separate physical-browser testing of an arbitrary unknown URL was not
  captured; this is a minor remaining validation caveat.
- **Failure semantics and safety:** temporary Wi-Fi/DNS/Internet/MTA/TLS
  failures do not erase/replace credentials or enter setup mode. The observed
  DNS failure recovered automatically. Failed credential replacement keeps
  prior credentials; only explicit confirmed forget clears them. Passwords
  are never printed/rendered. `include/secrets.h` exists locally, is ignored
  and untracked, and is not required for normal operation. Physical LED output
  remains disabled (`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`). No transport
  policy, LED behavior/mapping or manufacturing data was changed.
- **Validation:** Wi-Fi provisioning 310 checks / 0 failures; semantic tables
  302 map LEDs (276 STATION, 26 SEGMENT), 188 registry entities; TrainPipeline
  4,538 checks / 0 failures; feed transport 1,209 checks / 0 failures. All
  four builds passed: `brooklyn-s3` 34,264 B RAM / 306,481 B flash;
  `xiao-s3` 33,124 B / 286,529 B; `brooklyn-s3-network` 69,740 B /
  1,025,285 B; `xiao-s3-network` 68,628 B / 1,006,757 B. Default targets
  compile networking out; both network targets enable it. All retain
  `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`. `git diff --check` passed.
- **Checkpoint state:** the Phase 4L changes are committed and pushed on
  `main`, with annotated tag `phase-4l-wifi-provisioning` pushed to `origin`.
  No upload occurred during checkpointing. Verify live branch/tag state on
  resume rather than assuming the local repository has not advanced.
- **Remaining caveat / next action:** no physical-browser request to an
  arbitrary unknown URL was captured. This is minor and not a checkpoint
  blocker. No immediate action is required; optionally test that URL on the
  XIAO while on `BrooklynMap-Avi`. Before any future hardware interaction,
  verify the connected board and COM port. Production PCB remains untested.
- **Security:** no Wi-Fi passwords, tokens or credentials belong in this
  file. `include/secrets.h` is local, ignored, untracked and not needed for
  normal NVS operation; never stage or commit it.
- **Details:** `docs/PHASE4L_WIFI_PROVISIONING.md`.
