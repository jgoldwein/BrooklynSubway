# Brooklyn Map 0.2.35

- Fix "Unable to render status page" after signing in with full live train
  diagnostics. The settings plus diagnostic text exceeded the old 16 KiB
  output buffer. Use a shared 24 KiB page capacity in firmware and regression
  tests; the buffer remains static, adding 8 KiB of bounded RAM use.
- Reproduce the failure with all 18 route rows, eight station examples and
  signed-in production controls: 16,956 bytes. The same page with long escaped
  text fields renders at 17,797 bytes. Verify the former 16 KiB capacity fails
  and the new capacity preserves the complete diagnostics and settings.
- Preserve all train display rules, orange-light settings, OLED settings,
  authentication, firmware update controls and hardware behavior.

Validation: 460 LAN rendering/settings checks, 151 authentication checks, and
all four supported firmware build targets. The separate /update page can be
used to install this repair even while the old home page fails to render.
