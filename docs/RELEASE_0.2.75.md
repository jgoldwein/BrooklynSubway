# Brooklyn Subway 0.2.75

Rehearsal release to verify scheduled, unattended updates on COM6.
Adds the running firmware version to Overview, refreshed with live status.
Otherwise firmware behavior is unchanged from 0.2.74. Uses the same signing key,
production target, brightness limits and settings format.

The operator enabled Automatic updates and selected a 5-minute check interval
on COM6. No manual Check, Install or Restart action is used for this rehearsal.
Acceptance requires observing scheduled discovery, signed download, automatic
restart, version 0.2.75, successful boot health and resumed live train data.
These runtime results will be recorded separately after installation.
COM5 automatic updates remain off and its baseline test is not interrupted.
