# Manufacturing exports - October 9, 2026

The BOM and placement spreadsheets are the project owner's unchanged exports.
The public fabrication download is now an **art-free derivative** of the original
October archive. The original mapped ZIP is no longer distributed on the current branch.

## Downloads

- [BOM.xlsx](BOM.xlsx): bill of materials, including quantities, designators,
  footprints, manufacturer part numbers and supplier part numbers.
- [PickAndPlace.xlsx](PickAndPlace.xlsx): component coordinates, side, rotation
  and SMD flags. It also includes non-SMD/mechanical entries; do not treat every
  row as a part to be machine-placed.
- [Gerbers_Art_Free.zip](Gerbers_Art_Free.zip): original copper, masks, paste, outline and drill payloads, with empty top/bottom silkscreen and document layers. All colorful-artwork payloads are omitted. **All printed labels are absent**, not just the subway map.
- [art-free-integrity.json](art-free-integrity.json): exact source/output fingerprints and the list of preserved, cleared and omitted files.
- [SHA256SUMS.txt](SHA256SUMS.txt): checksums of the two unchanged spreadsheets and the new art-free ZIP.

## Revision and checks

The supplied filenames and Gerber generator headers identify these as
**October 9, 2026 exports**. They are grouped by export date rather than being
assigned an unverified hardware revision.

The BOM's ESP32-S3-WROOM-1U-N16R8 module and 306 addressable RGB LEDs agree with
the production hardware records. BOM quantities match their designator counts,
and the BOM designators occur in the placement export. The art-free Gerber derivative
passes its ZIP integrity check and retains every unaffected payload byte-for-byte. The spreadsheets were inspected without
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

## Artwork distribution change - November 19, 2026

The original Gerber SHA-256 was `90e487dd1cd208731659e0bb4dcc82b3d4df092b5b778cc3053aefc6b4ec29b6`. That source was preserved locally; the public derivative has a different checksum. No electrical geometry was edited. This archive has no printed component or control labels and is not an order-ready recommendation. It is **not** the matching baseline for the later working EasyEDA overlay project. See [artwork distribution notes](../../../docs/ARTWORK_DISTRIBUTION.md).
