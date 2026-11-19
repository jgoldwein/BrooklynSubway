# Editable blank builder template

The latest repository ZIP already includes the two required files in `inputs`.
**You do not need to download or copy them separately.** Supply your MTA PDF
locally and follow the [simple Windows workflow](../../tools/overlay/AUTOMATION.md#simplest-windows-route).

Individual downloads, if needed:

- [Blank_Map.eprj2](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Map.eprj2): native editable EasyEDA project, with its PCB, 11 attached schematic pages and component libraries.
- [Blank_Gerbers.zip](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Gerbers.zip): matching map-less colorful fabrication baseline. Keep it compressed.
- [verification.json](verification.json): source checksums and completed verification checks.

## What was cleaned

Seven PCB image/color graphic objects and their three embedded image resources were removed.
The front Wi-Fi setup QR code was then restored by the designer and is included. Other removed PCB graphics and logos remain absent. Existing
text labels, including setup IP/SSID instructions, remain. The personal serial
and one-of-a-kind description were made generic. Copyright attribution remains;
it is not a recipient personalization field. Account/password metadata was cleared
from the native distribution copy. All 11 schematic pages and their non-header
design records were retained, including the three resources used by schematic illustrations and library symbols.

## Verification and revision limits

Tested with EasyEDA Pro **3.2.149** and extension **0.1.3**: portable import,
native reopen, baseline export and a complete October-PDF overlay/export cycle.
Copper, masks, paste, outline and drilling were checked against the previous
working baseline; only the intended top/bottom printed artwork changed. The
mapped output preserved all non-color baseline payloads byte-for-byte.

This is the **builder working revision**. It is not byte-identical electrically
to the original October 9 manufactured archive, and does not replace the recorded
manufacturing-authority project. Review the design and fabrication preview before
ordering. A successful tool run is not manufacturing approval or artwork permission.

For this project's colorful map, select **White** PCB, **ENIG** surface finish,
and **Advanced Options → Silkscreen Technology → EasyEDA multi-color silkscreen**
on JLCPCB's quote page. Then open **Gerber Viewer** to inspect the uploaded result.
See the [step-by-step settings and preview instructions](../../tools/overlay/AUTOMATION.md#jlcpcb-settings-and-color-preview).

The project software license does not grant rights to MTA or other third-party
artwork. The MTA PDF and completed mapped Gerbers are not included. See
[artwork distribution notes](../../docs/ARTWORK_DISTRIBUTION.md).
