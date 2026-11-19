# Map artwork and public downloads

The current source tree no longer distributes standalone MTA map images,
map-based engineering screenshots or the original mapped fabrication archives.
The project owner's photographs and videos of the completed map are retained.
This is a distribution change, not a claim that all licensing questions are resolved.

## Build your own overlay

Obtain the map from the MTA and prepare it locally using the
[step-by-step builder guide](../tools/overlay/AUTOMATION.md).
Users are responsible for obtaining permissions required for their intended use.
The project's software license does not grant rights to third-party artwork.

## Historical engineering images

Map-based route/station review images were removed from the current branch.
Text, coordinates and mapping decisions remain as engineering records. Links
from affected review notes lead here instead of displaying the removed artwork.
The simulator now includes a simple original vector diagram of PCB LED locations,
not the MTA map. Users may load their own map image locally.

## Fabrication downloads

The [art-free October export derivative](../hardware/exports/2026-10-09/README.md)
retains the original circuit's copper, masks, paste, board outline and drills.
Its top/bottom silkscreen and document drawing layers are empty, and all four
proprietary colorful-artwork files are omitted. **All printed labels are removed**,
including useful control/component labels. It requires design review before ordering.
It is not the same circuit revision as the working overlay builder project and
must not be substituted as that project's matching baseline.

The original mapped archives are kept locally for engineering provenance, not
in current public downloads. The existing manufactured-authority project is unchanged.

## Earlier copies

On November 19, 2026, the identified artwork paths and historical silkscreen SVG
content were also removed from all published branch/tag histories. All 120
branch/tag names were retained. Photos, videos and firmware release assets were
preserved. A private pre-cleanup backup remains with the developer.

An audit checked all 74 published releases and 294 asset names; none indicated
Gerbers, overlay artwork or editable PCB projects. Release source archives now
follow the rewritten tags. Existing clones, forks, previously downloaded ZIPs,
old commit URLs and hosting caches can still retain earlier material; rewriting
the published refs cannot recall external copies or guarantee removal from caches.
