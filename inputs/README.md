# Included overlay build inputs

These files already come in the latest repository ZIP. **Do not copy or rename them.**

- **[Blank_Map.eprj2](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Map.eprj2):** clean native EasyEDA PCB project, with all 11 schematic pages and libraries retained.
- **[Blank_Gerbers.zip](https://github.com/jgoldwein/BrooklynSubway/raw/refs/heads/main/inputs/Blank_Gerbers.zip):** its verified matching map-less fabrication baseline. Keep it compressed.

The links above download the actual files if you need them individually. Normal
users should download the whole software ZIP so the folder layout is already correct.
See [template details and verification](../hardware/builder/README.md).

Double-click `Start_Overlay.cmd` in the repository folder. It downloads the PDF
directly from MTA and verifies its edition. If downloading fails, choose a locally
downloaded PDF in the fallback picker. You do not need to rename the PDF or put it
here. The optional manual command instead expects
`inputs/Source_Map.pdf`; user PDFs remain ignored by Git and are not redistributed.

Follow the [simple Windows workflow](../tools/overlay/AUTOMATION.md#simplest-windows-route).
Keep supplied originals unchanged; the helper creates a separate project copy
for each build. Edited/custom templates need a new matching baseline.
