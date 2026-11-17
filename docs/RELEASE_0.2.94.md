# Brooklyn Subway 0.2.94

Personalize the map under **Advanced > Personalization** after signing in.

- Set the map title, recipient, short OLED name, greeting, celebration date, From line, scrolling names, display creator credit, support name and email.
- The page previews the OLED greeting and explains text limits. The small OLED title uses the first 17 characters.
- Changes apply to the OLED and web interface without restarting. Support email opens a reviewed draft to the configured address; nothing is sent automatically.
- The celebration and Morse easter eggs use the configured name. Leaving the date empty disables the annual greeting.
- Settings survive restarts and firmware updates. Reset to firmware defaults restores the original Avi/Goldwein values and also clears Wi-Fi and administrator settings.
- The printed Wi-Fi setup network stays BrooklynMap-Avi at 192.168.4.1. Existing credentials, display behavior, quiet hours and physical pin assignments are preserved.
- Original project copyright, licenses and GitHub links remain unchanged.

Signed packages retain existing target checks, signature verification and automatic rollback health checks. Physical live output keeps the existing low brightness limit.

Validation: all four PlatformIO targets build; host checks cover storage save/reload and failures, escaped and maximum-length pages, authentication policy, custom greeting/date/Morse text, OLED runtime, support-email drafts, Wi-Fi setup and physical-output safety. COM6 USB flash hashes and startup health confirmation verified. Existing intermittent feed transport failures remain under investigation; this release does not claim to resolve them.
