# Brooklyn Subway hardware files

[Download the October 9, 2026 manufacturing exports](exports/2026-10-09/README.md):
Gerber ZIP, BOM and pick-and-place spreadsheets, with checksums and revision notes.

The spreadsheets are archived unchanged; the Gerber is a clearly labeled art-free derivative. See the export notes for the checks performed
and the distinction between an export date and a confirmed manufactured revision.

[Download the 3D-print files](3d-print/README.md): eight frame pieces, two OLED-case pieces and the 11 mm rotary knob, individually or as one ZIP.

[How the map was made](../docs/HOW_IT_WAS_MADE.md) | [Project home](../README.md)

**Care:** Keep the PCB out of direct sunlight. Read the [PCB handling and care instructions](../docs/AVI_USER_GUIDE.md#handling-cleaning-and-caring-for-the-pcb) before handling, cleaning or mounting it.

## Editable builder project

[Download the clean editable PCB project and matching blank Gerber](builder/README.md). Both are included in the latest software ZIP under `inputs`; all 11 schematic pages are retained.

## Reproduce the map artwork

The [overlay preparation tool](../tools/overlay/README.md) provides source hashing, crop/alignment, an offline landmark picker, LED previews and protected-layer verification/packaging. It uses an EasyEDA color export for final fabrication files. A calibrated PDF workflow has completed one live export; visual/manufacturing approval remains separate. See the [artwork distribution notes](../docs/ARTWORK_DISTRIBUTION.md).
