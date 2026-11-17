# Brooklyn Subway 0.2.96

Adds personalized controls for Celebration, Anniversary, Family Credits and Rangers under Advanced > Personalization. Each can be enabled or disabled; disabled extras are skipped in the TEST + ROUTE cycle. Messages and headings can be edited. Blank family heading uses the recipient name; blank Rangers caption uses the short OLED name followed by "'s favorite team". The Rangers effect is extended to 9.6 seconds with two repetitions of its horn/color sequence and a flashing OLED cheer, default GO RANGERS!, which is also editable. Horn pitches and brightness limits remain unchanged. Existing names, dedication, Wi-Fi and administrator settings are preserved.

The Anniversary switch also disables/enables the annual automatic greeting. Startup dedication remains separately controlled under Screen settings. Extras use a separate atomic NVS record in the existing reset-owned configuration namespace. Existing personalization records need no migration. Pending firmware-default resets now complete before saved personalization is loaded.

Automatic updates now default ON for new/no-saved-preference setups and after firmware-default resets. Explicit saved ON/OFF preferences remain intact. Boot checks still wait for Wi-Fi, clock and administrator readiness. Failed release checks now retry with 1/2/5/15-minute backoff, capped by the saved interval, rather than waiting the full interval after one failed attempt. Success restores the normal interval. Startup/failed-check serial diagnostics explain the state.

COM5's supplied screenshot shows 0.2.95 discovered while Automatic updates is OFF; it is awaiting installation approval. This release does not establish why an earlier check may have been delayed.

Signed package, target and rollback checks remain. No GPIO assignment, tone pitch, LED brightness limit or firmware dependency changes.

Validation: all four PlatformIO targets build. Host checks pass for extras save/reload/failure/corruption and isolation from the original personalization record; skipping disabled extras and custom messages; annual greeting suppression; cheer flashing and 9.6-second duration; escaped forms and authentication; update retry backoff and rollover; 76 OLED runtime scenarios; factory-reset policy; and physical-output safety. COM6 final USB flash hashes, saved personalization load and startup health confirmation passed. New extras appearance/listening feedback remains pending.
