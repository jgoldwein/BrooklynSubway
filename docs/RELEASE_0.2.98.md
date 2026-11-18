# Brooklyn Subway 0.2.98

## Generic, personalizable names and guides

- Avi's original firmware defaults are retained. The generic guides use **The Developer** for the saved support contact name. Names, greeting, From line, scrolling credits, date, creator and support contact remain editable under Advanced > Personalization.
- Saved personalization is preserved without changing its record format. Existing saved Avi profiles and contacts continue to work. Unsaved original defaults also remain available.
- Without a support email, Copy and Save report work and the email button stays disabled. There is no fallback to the original developer's personal email.
- The Rangers default and saved extra choices are retained. Its optional sound/colors remain specifically Rangers.
- README, current user/setup guides, GitHub Pages introduction and both printable PDFs use generic wording, with a new personalization guide. Canonical guides are USER_GUIDE.md and QUICK_START.md; the old guide paths remain as bookmark/QR-code links. Original authorship, licenses, build history and the fixed BrooklynMap-Avi setup-network contract are preserved.

No GPIO, power, feed-processing or alert-matching changes. Validation covers original defaults and generic guide wording, saved records and failed writes, legacy personalized OLED/anniversary/Morse behavior, custom contact email and unconfigured email handling, web escaping, OLED runtime and all four build targets.

COM6 hardware verification: firmware 0.2.98, saved personalization loaded, Fresh 8/8 feeds, OLED frame writes successful, startup health confirmed, Help links use the generic guide, and the support page retains the configured contact. No credentials or settings were erased. Both PDFs were rendered and checked; their QR codes decode to the canonical guides.
