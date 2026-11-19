# Overlay build inputs

This folder is included in the repository download. Put copies of these three
files here before running the overlay builder:

1. **Blank_Map.eprj2** - the editable, map-less EasyEDA Pro project.
2. **Blank_Gerbers.zip** - the matching blank fabrication ZIP exported from that
   same project. Keep it compressed.
3. **Source_Map.pdf** - your locally obtained, reviewed MTA map PDF matching the
   supplied crop recipe.

These filenames are used by the commands in the
[step-by-step guide](../tools/overlay/AUTOMATION.md#4-put-copies-of-your-inputs-in-one-place).
Renaming files does not make mismatched contents compatible.

The public blank builder pair is still being prepared. This folder does not
include an editable PCB project, a blank Gerber baseline or MTA artwork.
See the [input requirements](../tools/overlay/AUTOMATION.md#1-check-that-you-have-the-three-build-inputs).

Your input copies stay local and are ignored by Git. Keep originals elsewhere.

For the simplest route, double-click `Start_Overlay.cmd` in the repository folder. It checks for the PCB/Gerber pair before doing any setup, then lets you choose the PDF without renaming or copying it here. The `Source_Map.pdf` filename is only needed by the optional manual command.
