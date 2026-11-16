# Packing guide for Avi

Print **Brooklyn_Subway_Quick_Start.pdf** on **US Letter, portrait, one-sided, at 100% / Actual size**. Do not use a multiple-pages-per-sheet setting. Color is helpful; the written color labels also work when printed in black and white. Leave the map web-address line blank for Avi to fill after setup. Do not write passwords on the sheet.

The sheet is one page with an 11-point main body font, a blank web-address field and a vector QR code linking to the public full guide. The PDF embeds its fonts. It contains no sender network name, private LAN address, password, or firmware patch-version requirement.

- `guide_content.json`: canonical editable text and QR destination.
- `build_quick_start.py`: creates the PDF, Markdown companion, QR image, rendered preview and validation report from that content.
- `Brooklyn_Subway_Quick_Start.md`: editable, phone-readable text companion. Regeneration replaces it; edit the JSON to keep PDF and Markdown in sync.
- `Brooklyn_Subway_Quick_Start_preview.png`: final rendered PDF preview for visual review.
- `validation.json`: one-page/text/QR checks; final release QA also checked the public link and inspected the rendered page.

## Rebuild on Windows

These are document-only dependencies, not PlatformIO or firmware dependencies:

```powershell
python -m pip install -r docs/shipping/requirements.txt
python docs/shipping/build_quick_start.py
```

The builder uses Windows Segoe UI fonts from `C:\Windows\Fonts` and embeds them. Set `BROOKLYN_PDF_FONT_DIR` to another directory containing `segoeui.ttf` and `segoeuib.ttf` if needed. `BROOKLYN_PDF_DEPS` may point to an isolated dependency directory instead of installing globally. For this build, dependencies were installed in a temporary folder; project firmware dependencies were not changed.

PDF rendering uses PyMuPDF because Poppler was unavailable in this environment. The builder verifies one US Letter page, required instructions and private-data exclusions, renders the actual PDF to PNG, then decodes that page's QR code to check the destination. After any content or layout edit, inspect the newly rendered preview before printing. If wording changes increase the page height, the builder rejects content outside its lower printable area.

## Before packing

The shipping board must be prepared to open setup Wi-Fi instead of using the sender's saved credentials. That preparation belongs in the separate shipping checklist; this guide deliberately starts with Avi's first-time setup. Use the supplied tested adapter and retain the external antenna. The printed guide does not require Avi to wait for network-ready lights before connecting the map to his Wi-Fi.
