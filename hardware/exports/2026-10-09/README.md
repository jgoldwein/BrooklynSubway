# Manufacturing exports - October 9, 2026

These are the project owner's supplied manufacturing exports, preserved
byte-for-byte. The short filenames below make downloading easier.

## Downloads

- [BOM.xlsx](BOM.xlsx): bill of materials, including quantities, designators,
  footprints, manufacturer part numbers and supplier part numbers.
- [PickAndPlace.xlsx](PickAndPlace.xlsx): component coordinates, side, rotation
  and SMD flags. It also includes non-SMD/mechanical entries; do not treat every
  row as a part to be machine-placed.
- [Gerbers.zip](Gerbers.zip): fabrication archive, including copper, solder mask,
  silkscreen, paste, outline, drill and colorful-silkscreen export files.
- [SHA256SUMS.txt](SHA256SUMS.txt): checksums of the three unchanged files.

## Revision and checks

The supplied filenames and Gerber generator headers identify these as
**October 9, 2026 exports**. They are grouped by export date rather than being
assigned an unverified hardware revision.

The BOM's ESP32-S3-WROOM-1U-N16R8 module and 306 addressable RGB LEDs agree with
the production hardware records. BOM quantities match their designator counts,
and the BOM designators occur in the placement export. The Gerber archive
passes its ZIP integrity check. The spreadsheets were inspected without
executing formulas or changing their contents.

These checks do not establish that every trace, placement, footprint or artwork
layer is identical to the final manufactured V1.1 design. The firmware's
hardware authority remains `Brooklyn_Active_Subway_Map_V1.1.eprj2`, recorded in
the project context with SHA-256
`3ecb90c15d79f027a949c047f9c0a6e2f544706750498f9c7cb94209d61b4bc9`.
Confirm the intended revision and assembly selections before ordering a board.
The editable manufacturing-authority file is not included in this upload.

## Original filenames

- `BOM_Brooklyn MTA Active Map_Brooklyn MTA Active Map PCB_2026-10-09.xlsx`
- `PickAndPlace_Brooklyn MTA Active Map PCB_2026_10_09.xlsx`
- `Gerber_Brooklyn_MTA_Active_Map_PCB_2026-10-09.zip`

A schematic PDF and assembly instructions have not been included in this set.
Third-party material retains its original rights; see
[the project notices](../../../THIRD_PARTY_NOTICES.md).
