# Reproducing the map overlay

This is a **host-side artwork preparation tool**, not firmware. The preparation and packaging commands do not edit an EasyEDA project or the submitted Gerber archive. The experimental extension temporarily inserts artwork into a separate project copy and removes it after export; it does not save the source project or change copper, holes or routing.

**Want the automated workflow?** Start with [Install and use Brooklyn Overlay Builder](AUTOMATION.md). It explains the one-time EasyEDA extension installation, local helper and result checks. Live editor integration is still awaiting verification; the manual workflow below remains available.

**Current status:** a PDF crop recipe has now been recovered for the preserved September 2026 edition, with explicit placement from the owner's visually checked replacement image. The owner has now visually confirmed the generated PDF-derived image in the board preview. The recipe remains **DRAFT** for manufacturing: the final export and alignment against the original manufactured board still require review. The source working project differs electrically from the manufactured Gerbers. The original fabrication archive remains the electrical baseline. No order-ready replacement archive or manufacturing approval is supplied.

## What has been established

- The owner's supplied October 9 Gerber ZIP is byte-identical to `hardware/exports/2026-10-09/Gerbers.zip` (SHA-256 below).
- The manufacturer production archive contains two embedded source ZIPs. Every one of their 20 file payloads matches the submitted archive. The production archive itself is kept private; its order metadata is not needed by this tool.
- This establishes which fabrication export was submitted. It does **not** establish whether the source EasyEDA project was called V1.0 or V1.1. The recorded firmware hardware authority is unchanged.
- The supplied placement workbook contains 306 LED designators. Coordinates are read directly from that workbook, not inferred from the firmware's virtual-map calibration.
- The design outline uses millimeters and absolute 4.5-format coordinates. Its **centerline** bounding rectangle is X -10.712 to 351.495 mm and Y -8.674 to 307.810 mm: 362.207 by 316.484 mm, including its notch. A Gerber viewer's stroked bounding box can be larger by the outline aperture radius; it is not the same measurement.

Submitted archive SHA-256:

```text
90e487dd1cd208731659e0bb4dcc82b3d4df092b5b778cc3053aefc6b4ec29b6
```

## Install

For PCB image placement and colorful fabrication export, install the **EasyEDA Pro desktop application** from the [official download page](https://easyeda.com/page/download). Choose **EasyEDA Pro Edition**, not EasyEDA Standard. The automated extension workflow was tested with Pro 3.2.149; see the [installation guide](AUTOMATION.md) for version notes and the complete setup sequence.

Use Python 3.12 or newer (tested with Python 3.13) from the repository root. Windows PowerShell example:

```powershell
python -m venv .venv-overlay
.\.venv-overlay\Scripts\python -m pip install -r tools/overlay/requirements.txt
.\.venv-overlay\Scripts\python tools/overlay/test_overlay.py
```

The pinned host dependencies are Pillow 13.0.0 (raster handling), NumPy 2.5.4 (affine fitting), and PyMuPDF 1.26.5 (PDF/SVG rendering). They are not added to firmware. Use `python tools/overlay/overlay.py --help` for commands; replace `python` below with your virtual-environment Python when needed. Outputs never overwrite existing paths.

### Optional crop-recovery analysis

Recovering an unknown crop can use **opencv-python-headless 4.13.0.92** for SIFT feature matching and robust affine estimation. This is an optional host-only calibration dependency, installed in an isolated analysis directory or virtual environment. It is not used by firmware or required by the normal preparation commands. Feature matches produce a candidate registration; held-out points, broad image coverage and visual review are still required. Do not infer manufacturing approval from a feature match.

## Quick path: prepare the recovered Brooklyn crop

The recipe `recipes/brooklyn-september-2026.json` records the crop and the owner-checked replacement placement. It contains numbers and source hashes, not MTA artwork. Use the preserved PDF whose SHA-256 is:

```text
93f58f809c48832dad4688710e8b848811581119b12d2cf0c307c31c779b0423
```

The [MTA source URL](https://www.mta.info/map/5256) currently serves a different edition. The tool rejects that newer file rather than silently applying old registration to changed artwork. It does not download or bundle the old PDF.

```powershell
python tools/overlay/overlay.py prepare-pdf "YOUR_MATCHING_ORIGINAL_MAP.pdf" --recipe tools/overlay/recipes/brooklyn-september-2026.json --output captures/my-prepared-map
```

This one command produces `prepared-map.png`, `placement.json`, `EASYEDA_IMPORT.md` and a hash manifest. Default output is 300 DPI, 4043 by 3517 pixels. No Gerber or source project is modified.

1. Open a separate copy of the blank-map EasyEDA project. Preserve its QR code, dedication and custom labels.
2. Place the generated PNG with original quality selected on **Top Silkscreen Layer**. Click once to place it temporarily, then select it and enter the numeric properties below. The PNG contains artwork and DPI, not EasyEDA board coordinates; it cannot position itself on the PCB.
3. Enter both dimensions: **342.310 mm wide, 297.810 mm high**. The independent horizontal/vertical scaling deliberately reproduces the owner-checked fit; do not force the PDF's original aspect ratio.
4. Enter **Start X 0.000 mm, Start Y 297.810 mm, rotation 0, mirror No**. These are EasyEDA image-property values, not a proven generic Gerber origin conversion.
5. Inspect top, center, bottom and far-edge LED/station alignment and the custom labels in the color preview. Export colorful fabrication files only after that check.

The crop was recovered from the 2852 by 2665 intermediate image at pixel rectangle `[1589,2732,4441,5397]` inside a 4600 by 5600 full-map raster, corresponding to PDF points `[572.04,983.52,1598.76,1942.92]`. Spatial image matching supported the recovery, including 14 held-out features. These checks do not replace station-by-station review. The clean PDF output does not reproduce manual annotations embedded in the earlier JPEG.

### Depersonalize before reinserting the map

For a reusable blank template, remove or change personal markings in the EasyEDA source first, then export that blank project. Use it as the artwork-comparison baseline. Insert the prepared map into a copy of that same depersonalized project and export a matching mapped candidate. Do not transplant the color files from an older personalized mapped export: they can restore the removed names and other markings. Inspect both front and back previews. Selecting an artwork template does not establish that its circuitry matches the original manufacturing archive; keep that comparison separate.

### Blank-map workflow established with the owner

Work on copies. First export an unchanged project, then remove only the selected map image and export a blank candidate. In this case the QR code, logo, dedication, OLED area and control labels were separate and remained visible. Preserve the original map object, its image file, and its position/size record before deletion.

The tested working project had the same outline and 304 of 306 identical LED positions, but different vias and copper from the original manufacturing archive. **Do not use its new electrical layers to replace the manufactured board.** Blank/remapped exports were checked against each other: their protected drawing commands were identical after expanding renumbered aperture references and excluding unused aperture definitions. The public verifier now recognizes this narrow equivalence. All remaining drawing commands, polarity commands and macro bodies must match exactly. Aperture redefinitions, aperture blocks and unsupported selection syntax fail closed; actual width, coordinate or geometry changes are still rejected. Packaging an original-board blank template still requires review of the color content and a controlled merge that retains every original electrical payload.

## 1. Inspect and pin the board export

```powershell
python tools/overlay/overlay.py inspect hardware/exports/2026-10-09/Gerbers.zip --placements hardware/exports/2026-10-09/PickAndPlace.xlsx --output captures/my-board.json
```

The report lists every file hash, the outline and LED coordinates. The parser intentionally rejects unfamiliar outline units, formats, arcs and multiple paths rather than approximating them. This implementation targets the supplied design export; it is not a general Gerber CAM editor.

If you have a JLCPCB production download, identify embedded submitted-source archives separately:

```powershell
python tools/overlay/overlay.py production-audit --production YOUR_PRODUCTION_DOWNLOAD.zip --submitted hardware/exports/2026-10-09/Gerbers.zip --output captures/production-source-check.json
```

Do not use manufacturer CAM outputs as replacement design files. They can contain different coordinate origins, process rails and production adjustments. Reports can include local production member names: review them before sharing. No archives are extracted to filesystem paths.

## 2. Obtain a specific source map

Start at the [official MTA maps page](https://www.mta.info/maps). Select the same diagram edition and layout used by the calibration. Record its SHA-256 from a known, reviewed copy; a hash verifies identity, not rights or authenticity by itself. Do not substitute the latest map: stations, labels and route geometry can change.

```powershell
Get-FileHash -Algorithm SHA256 YOUR_REVIEWED_MAP.pdf
python tools/overlay/overlay.py fetch --url "https://www.mta.info/YOUR_EXACT_MAP_URL" --sha256 YOUR_REVIEWED_64_CHARACTER_HASH --output captures/map-source.pdf
```

The URL and hash above are placeholders, not a supplied production source. Fetch accepts HTTPS MTA hosts and rejects redirects outside those hosts. If the original reviewed edition is unavailable online, use a legitimately obtained local copy. A mismatch fails rather than accepting another map edition.

The tool does not bundle source MTA artwork, register artwork rights, or make a legal determination. Obtaining an image separately does not itself establish reproduction permission. Follow the project's third-party notices and your applicable permissions review.

## 3. Rasterize and crop

```powershell
python tools/overlay/overlay.py rasterize captures/map-source.pdf --sha256 PDF_HASH --page 1 --dpi 300 --output captures/map-full.png
Get-FileHash -Algorithm SHA256 captures/map-full.png
python tools/overlay/overlay.py crop captures/map-full.png --sha256 RASTER_HASH --box LEFT TOP RIGHT BOTTOM --output captures/map-crop.png
```

`--box` requires four integer **source-pixel** coordinates. PDF page numbers start at 1. Choose the Brooklyn region broadly enough to include every intended map LED. PDF and flattened, self-contained SVG inputs are supported; external SVG resources and active content are rejected. Rasterization/crop sidecars record source/output hashes, page, renderer or crop rectangle. Retain those sidecars with your calibration. PNG/JPEG images can be used directly for picking. No AI image editing is used.

## 4. Pick and fit landmarks once

```powershell
python tools/overlay/overlay.py pick --source captures/map-crop.png --gerber hardware/exports/2026-10-09/Gerbers.zip --placements hardware/exports/2026-10-09/PickAndPlace.xlsx --output captures/pick-landmarks.html
```

Open that HTML file locally in a browser. It embeds your image, operates offline and sends nothing. Select an LED designator, inspect its highlighted board position, then click the intended location on the source image. Use map LEDs, not the four status-panel LEDs, when calibrating a map-only image. Use zoom for precision. Pick at least six distinct LEDs spread across the top, bottom, left, right and center, then download `landmarks.json`. Anchor spans must cover at least 45% of both source-image dimensions; crop a whole-city map first rather than calibrating a tiny clustered region.

```powershell
python tools/overlay/overlay.py fit --source captures/map-crop.png --landmarks YOUR_DOWNLOADED_landmarks.json --gerber hardware/exports/2026-10-09/Gerbers.zip --placements hardware/exports/2026-10-09/PickAndPlace.xlsx --tolerance-mm 0.35 --output captures/map-profile.json
```

The profile pins the source raster, Gerber and workbook hashes. It stores the affine matrix and maximum anchor residual in millimeters. Fit rejects collinear, clustered, repeated, mirrored or inconsistent points. **A small fitting residual only describes the selected anchors.** Independently check additional stations, shared-line LED lanes, terminals and all edges. Do not raise the tolerance merely to make a wrong map fit. This uses a single affine transformation; it does not reproduce unidentified local warps or edits.

Profiles are draft records, not automatically approved registrations. Once a specific source, crop and profile have been reviewed, preparation is repeatable without picking again.

## 5. Prepare the import image

```powershell
python tools/overlay/overlay.py prepare --source captures/map-crop.png --profile captures/map-profile.json --gerber hardware/exports/2026-10-09/Gerbers.zip --placements hardware/exports/2026-10-09/PickAndPlace.xlsx --dpi 300 --output captures/prepared-map
```

Outputs:

- `prepared-artwork.png`: transformed artwork clipped to the design outline, including the notch; no review labels are printed into it.
- `alignment-review.png`: separate top-view preview with LED designators and board outline.
- `placement-reference.svg`: image with explicit physical dimensions, for placement reference rather than direct fabrication.
- `EASYEDA_IMPORT.md`: dimensions and Gerber-coordinate registration instructions.
- `manifest.json`: source, profile and output hashes; no manufacturing-approval flag is set true.

The image's full-board bounding rectangle is deliberate. Set both physical dimensions explicitly; PNG DPI alone is not an origin/placement specification. Gerber Y increases upward; image Y increases downward. The script performs that conversion and rejects reflections. An EasyEDA project origin can differ from the Gerber origin; verify that offset against known LEDs. Image pixels introduce finite resolution and interpolation: this is not a bit-identical regeneration of opaque original color files.

## 6. Export color fabrication files with EasyEDA Pro

Follow [JLCPCB's color-silkscreen instructions](https://jlcpcb.com/help/article/how-to-design-multi-color-silkscreen-using-easyeda): enable its color process, import the prepared PNG at original quality, and export color fabrication files. Use a COPY of the correct project. Replace only the map image; retain the custom legends, component labels, OLED area, QR codes, other artwork, and pad/hole clearances.

A downloaded map alone does not recreate those custom additions. This tool does not edit a project, convert PNG into FCTS/FCBS, decode those opaque files or automate EasyEDA clicks. The final map registration and full-color production preview need review before ordering.

## 7. Verify and package

```powershell
python tools/overlay/overlay.py verify --baseline hardware/exports/2026-10-09/Gerbers.zip --candidate YOUR_NEW_EASYEDA_EXPORT.zip --output captures/export-check.json
python tools/overlay/overlay.py package --baseline hardware/exports/2026-10-09/Gerbers.zip --candidate YOUR_NEW_EASYEDA_EXPORT.zip --output captures/rebuilt-gerbers
```

`verify` exits 2 when protected content differs. It allows changes only to `.FCTS`, `.FCBS`, `.FCBO` and `.FCBM`; other members must match, apart from comments/line endings, the standard Gerber creation-date attribute, and the narrowly checked aperture renumbering/unused-definition equivalence described above. It does not claim general geometric equivalence between differently expressed Gerbers. Normal top/bottom silkscreen GTO/GBO are protected too: if EasyEDA changes their drawing content, stop and review instead of weakening the check. Added or missing members also fail.

`package` first runs those checks, then creates `Gerbers-with-prepared-artwork.zip` and `integrity-report.json`. Only the new opaque color files are taken from the candidate; **every other file's payload comes from the original archive, byte-for-byte**. It verifies that property before writing the output. A passing integrity report is not artwork alignment approval, electrical sign-off, legal clearance or manufacturer acceptance. Review JLCPCB's production preview before ordering.

## Validation performed

- 25 synthetic tests: outline/CPL parsing, source hashes and download-host checks, SVG rendering/resource rejection, crop bounds, affine fitting, mirrored/tampered profiles, protected-layer changes, unsafe ZIP members, embedded source comparisons, no-overwrite behavior, PDF recipe crops and geometry guards, aperture equivalence/rejection cases and package payload preservation.
- Submitted and manufacturer-embedded source files compared: all 20 payloads identical in both embedded source archives.
- Read-only real-board coordinate exercise: 302 existing board-preview reference points fitted to the supplied placement workbook, maximum residual about 0.000631 mm; full-board LED preview generated and inspected. This checks coordinate plumbing and is **not** a fit of a newly downloaded MTA map or independent proof of the source artwork's identity.
- Original local PDF and edited SVG rasterization exercised. Identity packaging of the actual 20-member design archive also preserved every file payload. Generated artwork and production-order metadata are kept out of this tool's public files.

The remaining production task is to reconstruct and independently review the exact original source-map crop/registration and custom-artwork composition. No order-ready replacement Gerber is supplied with this initial tool.


### Packaging a matched depersonalized pair

After adding the map to a copy of your selected depersonalized blank project, export its mapped candidate and run `package` with the **depersonalized blank** as `--baseline`. The output takes only the four color-fabrication payloads from the mapped candidate; all 16 other payloads come byte-for-byte from the chosen blank. The integrity report records that property. The chosen blank defines the circuitry for this package. If it differs from the original manufactured board, this operation does not resolve or approve those electrical differences. Review both front and back color previews before fabrication.
