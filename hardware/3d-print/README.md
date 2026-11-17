# Brooklyn Subway 3D-print files

Joel designed and 3D-printed the PCB frame, OLED case and rotary knob for this map.

**[Download all 11 STL files as a ZIP](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/hardware/3d-print/Brooklyn_Subway_3D_Print_Files.zip)**

## Parts included

For one set, the supplied files provide one of each of the following pieces:

- Four frame bottom pieces: UL1, UR1, LR1 and LL1.
- Four matching frame top pieces: UL1, UR1, LR1 and LL1.
- OLED Base and OLED Top - no label.
- Rotary Swtich Knob - 11mm (the original filename spelling is preserved).

The corner labels are those supplied by the designer. Match each top and bottom by its corner label and check the fit against your board before assembly.

## Individual files

Click an STL to open its GitHub file page, where you can preview or download it.

- [Brooklyn Map - Frame Bottom LL1](<Brooklyn Map - Frame Bottom LL1.stl>)
- [Brooklyn Map - Frame Bottom LR1](<Brooklyn Map - Frame Bottom LR1.stl>)
- [Brooklyn Map - Frame Bottom UL1](<Brooklyn Map - Frame Bottom UL1.stl>)
- [Brooklyn Map - Frame Bottom UR1](<Brooklyn Map - Frame Bottom UR1.stl>)
- [Brooklyn Map - Frame Top LL1](<Brooklyn Map - Frame Top LL1.stl>)
- [Brooklyn Map - Frame Top LR1](<Brooklyn Map - Frame Top LR1.stl>)
- [Brooklyn Map - Frame Top UL1](<Brooklyn Map - Frame Top UL1.stl>)
- [Brooklyn Map - Frame Top UR1](<Brooklyn Map - Frame Top UR1.stl>)
- [Brooklyn Map - OLED Base](<Brooklyn Map - OLED Base.stl>)
- [Brooklyn Map - OLED Top - no label](<Brooklyn Map - OLED Top - no label.stl>)
- [Brooklyn Map - Rotary Swtich Knob - 11mm](<Brooklyn Map - Rotary Swtich Knob - 11mm.stl>)

## Before printing or fitting

- STL files do not encode physical units. These are intended as millimeter-scale parts; verify the imported size in your slicer before printing. As a useful reference, the OLED Base bounding box is 73 x 50.5 x 12.7 file units. This is its overall size, not its internal screen opening.
- No slicer profiles, filament, support settings or print orientations were supplied with this archive. Choose settings for your printer and check the preview; these files have not been newly test-printed as part of this upload.
- Remove barrel power and USB before fitting parts. Check clearances around components, buttons, the antenna cable and connectors. Do not force the knob onto the rotary shaft or let the frame press on components.
- Keep the OLED mounting magnet and the antenna fixed in their intended positions. See [PCB handling and care](../../docs/AVI_USER_GUIDE.md#handling-cleaning-and-caring-for-the-pcb).

## Archive and checks

The 11 supplied STLs are archived byte-for-byte unchanged. Basic validation checked binary STL lengths, triangle counts and finite coordinates; this does not certify mesh watertightness, printability or compatibility with other board revisions. [SHA256SUMS.txt](SHA256SUMS.txt) contains a checksum for each original STL.

These original mechanical designs are covered by the project's [hardware license](../../LICENSES/CERN-OHL-P-2.0.txt). Third-party artwork and trademarks retain their own terms.

[Hardware files](../README.md) | [How the map was made](../../docs/HOW_IT_WAS_MADE.md) | [Project home](../../README.md)
