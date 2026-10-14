# Ambiguous 13 LED Human Review

**Superseded by human confirmation on 2026-10-13.** The review alternatives
below are preserved as historical review context, not semantic assignments.
All 13 LEDs have since been promoted as HUMAN_CONFIRMED / CONFIRMED; see the
[current human-review queue](HUMAN_REVIEW_QUEUE.md) and
[remaining-LED analysis](../analysis/REMAINING_LED_DEEP_ANALYSIS.md) for the
authoritative mappings and refreshed counts.

The red numbered rings in the three crops mark the accepted CPL-to-raster
positions. Green rings mark nearby currently verified anchors. All artwork
pixels are native-resolution crops of the final 4096 x 3578 V1.1 image.

## Recommended order and decision difficulty

| Order | LED(s) | Category | Review focus |
|---:|---|---|---|
| 1 | LED277, LED278 | BINARY_EASY | Which point of the Beach 90 pair lies on the blue A lane versus the gray S lane? |
| 2 | LED279, LED280 | BINARY_EASY | Repeat the lane check for the Beach 98 pair. |
| 3 | LED269, LED270 | BINARY_CLOSE | At Broad Channel, distinguish the A station lane from the shuttle branch where the lines meet. |
| 4 | LED281, LED282 | BINARY_CLOSE | Resolve the paired points where the Beach 105 lanes draw close together. |
| 5 | LED283, LED284 | BINARY_CLOSE | At the shared terminal, determine which footprint belongs to A and which to S. |
| 6 | LED136 | STATION_VS_SEGMENT | Is the point a route-line segment, a station/interchange marker, or another transfer element? |
| 7 | LED191 | MULTIWAY | Identify the route lane and whether the point is a station node, segment, or transfer connector. |
| 8 | LED290 | MULTIWAY | Identify the B/Q lane and whether the point is a segment or another corridor element. |

There are **4 BINARY_EASY**, **6 BINARY_CLOSE**, **2 MULTIWAY**, **1
STATION_VS_SEGMENT**, and **0 OTHER** LEDs.

## BINARY_EASY — Beach 90 and Beach 98

At Beach 90 and Beach 98, the defensible station alternatives for each
individual footprint are **A / STATION** or **S / STATION** at the printed
station. The pair at each station is expected to split between the blue A
lane and gray Rockaway Park Shuttle lane; both permutations remain possible
until the exact centers are inspected. If an apparent A point is not on a
continuous A lane, note that rather than forcing an A assignment.

### LED277 — BINARY_EASY

**Physical context:** CH8, channel index 11; CPL (318.389, 188.087) mm;
projected raster (3708.9112, 1355.9515) px.

**Alternative A:** A / STATION / Beach 90 St. The exact center falls on the
blue A lane at the printed Beach 90 marker.

**Alternative B:** S / STATION / Beach 90 St. The exact center falls on the
gray shuttle station lane at the same printed marker.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275); the artwork's A sequence
continues toward Broad Channel. The neighboring pair member is LED278.
LED269/LED270 mark the unresolved Broad Channel junction; they are possible
transfer companions, not confirmed anchors. No local S station in this crop
is currently confirmed.

**Sequence constraints:** S / Beach 90 follows the Broad Channel shuttle
branch and precedes Beach 98. An A assignment is valid only if the exact
point is on the continuous blue A lane; placing an A station dot on the gray
branch would break route continuity. The pair should not be assigned to the
same route if the artwork shows the two route lanes.

**Human question:** At the Beach 90 label, which lane passes through the
center of red marker 1: blue A or gray S?

### LED278 — BINARY_EASY

**Physical context:** CH8, channel index 10; CPL (320.421, 185.928) mm;
projected raster (3731.7151, 1380.1839) px.

**Alternative A:** A / STATION / Beach 90 St. The exact center falls on the
blue A lane at the printed Beach 90 marker.

**Alternative B:** S / STATION / Beach 90 St. The exact center falls on the
gray shuttle station lane at the same printed marker.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275). The paired footprint is
LED277. LED269/LED270 at Broad Channel are unresolved possible route/transfer
companions. There is no currently confirmed local S station anchor.

**Sequence constraints:** S / Beach 90 belongs between Broad Channel and
Beach 98. An A assignment must follow the continuous blue lane rather than
jump onto the gray shuttle branch. Together LED277 and LED278 should split
between A and S if they mark the two printed lanes.

**Human question:** At the Beach 90 label, which lane passes through the
center of red marker 2: blue A or gray S?

### LED279 — BINARY_EASY

**Physical context:** CH8, channel index 8; CPL (310.007, 180.213) mm;
projected raster (3614.8292, 1444.3249) px.

**Alternative A:** A / STATION / Beach 98 St. The exact center falls on the
blue A lane at the printed Beach 98 marker.

**Alternative B:** S / STATION / Beach 98 St. The exact center falls on the
gray shuttle station lane at the same printed marker.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275); neither is a local S anchor.
The paired footprint is LED280. The preceding S candidate pair is LED277 /
LED278 at Beach 90, and the following pair is LED281 / LED282 at Beach 105.
These are unconfirmed sequence points.

**Sequence constraints:** S / Beach 98 follows Beach 90 and precedes Beach
105. A station assignment must follow the blue A lane continuously. The
LED279/LED280 pair should split between route lanes if both footprints align
to distinct lanes.

**Human question:** At Beach 98, is red marker 3 centered on the blue A lane
or the gray S lane?

### LED280 — BINARY_EASY

**Physical context:** CH8, channel index 9; CPL (312.166, 178.054) mm;
projected raster (3639.0586, 1468.5574) px.

**Alternative A:** A / STATION / Beach 98 St. The exact center falls on the
blue A lane at the printed Beach 98 marker.

**Alternative B:** S / STATION / Beach 98 St. The exact center falls on the
gray shuttle station lane at the same printed marker.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275). The paired footprint is
LED279. The preceding S candidate pair is LED277 / LED278 (Beach 90); the
following pair is LED281 / LED282 (Beach 105). None of these are confirmed
anchors.

**Sequence constraints:** S / Beach 98 fits between Beach 90 and Beach 105.
An A assignment must remain on the continuous blue route. If the pair marks
both lanes, LED279 and LED280 should receive different route labels.

**Human question:** At Beach 98, is red marker 4 centered on the blue A lane
or the gray S lane?

## BINARY_CLOSE — Broad Channel, Beach 105, and Rockaway Park terminal

For each footprint below, the station alternatives are A / STATION or
S / STATION at the printed stop. Pair identity is not established by LED
number or chain order. The human should trace the artwork lane through the
projected center.

### LED269 — BINARY_CLOSE

**Physical context:** CH8, channel index 19; CPL (306.070, 220.853) mm;
projected raster (3570.6804, 988.1933) px.

**Alternative A:** A / STATION / Broad Channel. The point is on the blue A
station lane at the Broad Channel marker.

**Alternative B:** S / STATION / Broad Channel. The point is on the gray
Rockaway Park Shuttle lane at Broad Channel.

**Nearby confirmed anchors:** A / Beach 67 St (LED276) and A / Beach 60 St
(LED275) are on the nearby mainland-bound A sequence. LED270 is the paired
Broad Channel footprint. The gray S branch begins at Broad Channel and
continues toward Beach 90; neither branch footprint is confirmed.

**Sequence constraints:** Broad Channel is the A/S junction. Assigning both
LED269 and LED270 to one route would leave the other visible route lane
without its paired station footprint if both points align to separate lanes.
The center at a junction can look like a connector, so inspect the printed
station node as well as lane color.

**Human question:** Which Broad Channel footprint is centered on the blue A
station lane, and which is centered on the gray shuttle branch?

### LED270 — BINARY_CLOSE

**Physical context:** CH8, channel index 20; CPL (304.038, 218.821) mm;
projected raster (3547.8725, 1010.9994) px.

**Alternative A:** A / STATION / Broad Channel. The point is on the blue A
station lane at the Broad Channel marker.

**Alternative B:** S / STATION / Broad Channel. The point is on the gray
Rockaway Park Shuttle lane at Broad Channel.

**Nearby confirmed anchors:** A / Beach 67 St (LED276) and A / Beach 60 St
(LED275); LED269 is the paired Broad Channel footprint. The S branch heads
from this junction to Beach 90, then Beach 98, Beach 105, and the terminal;
those candidate stop LEDs are not confirmed.

**Sequence constraints:** Same pair constraint as LED269: the two centers
should split across A and S if the artwork shows separate route-specific
station lanes. Do not decide the assignment from proximity to a route label
or the package order.

**Human question:** Is red marker 2 on the blue A station lane or on the gray
S branch at Broad Channel?

### LED281 — BINARY_CLOSE

**Physical context:** CH8, channel index 6; CPL (301.244, 172.085) mm;
projected raster (3516.4710, 1535.5491) px.

**Alternative A:** A / STATION / Beach 105 St. The point is on a continuous
blue A lane at the printed station.

**Alternative B:** S / STATION / Beach 105 St. The point is on the gray
shuttle lane at the printed station.

**Nearby confirmed anchors:** The nearest confirmed local A stations remain
Beach 67 St (LED276) and Beach 60 St (LED275). LED282 is the paired Beach
105 footprint; LED279/LED280 at Beach 98 precede it and LED283/LED284 at
Rockaway Park-Beach 116 follow it on the S branch. Those candidate points
are unconfirmed.

**Sequence constraints:** S / Beach 105 follows S / Beach 98 and precedes the
Rockaway Park terminal. An A assignment is only consistent if the artwork
shows the point on the continuous blue A route. The lanes are close here, so
the projected center matters more than the ring edge.

**Human question:** At Beach 105, does the center of red marker 5 sit on the
gray S stop lane or a blue A lane?

### LED282 — BINARY_CLOSE

**Physical context:** CH8, channel index 7; CPL (303.403, 170.053) mm;
projected raster (3540.7004, 1558.3562) px.

**Alternative A:** A / STATION / Beach 105 St. The point is on a continuous
blue A lane at the printed station.

**Alternative B:** S / STATION / Beach 105 St. The point is on the gray
shuttle lane at the printed station.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275). LED281 is the paired Beach
105 footprint; LED279/LED280 are the preceding Beach 98 candidate pair and
LED283/LED284 the following terminal pair.

**Sequence constraints:** S / Beach 105 must fit between Beach 98 and
Rockaway Park-Beach 116. An A assignment must trace the blue A route
continuously. Check whether the other member LED281 occupies the other
visible lane rather than assuming a route split.

**Human question:** At Beach 105, which lane passes through the center of
red marker 6?

### LED283 — BINARY_CLOSE

**Physical context:** CH8, channel index 5; CPL (292.608, 164.211) mm;
projected raster (3419.5383, 1623.9225) px.

**Alternative A:** A / STATION / Rockaway Park-Beach 116 St. The point is on
the blue A terminal lane at the shared printed terminal.

**Alternative B:** S / STATION / Rockaway Park-Beach 116 St. The point is on
the gray shuttle terminal lane at the shared printed terminal.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275). LED284 is the paired terminal
footprint; LED281/LED282 at Beach 105 are the preceding S candidate pair.
The printed terminal carries both A and S symbols, so those symbols alone do
not distinguish these two physical LEDs.

**Sequence constraints:** The S sequence ends here after Beach 105. A is
plausible only if the exact footprint aligns to the A terminal lane. The
shared terminal geometry means a duplicate A or duplicate S assignment
would conflict with the two-lane interpretation if both centers mark
different lanes.

**Human question:** At the Rockaway Park-Beach 116 terminal, is red marker 7
on the blue A terminus or the gray S terminus?

### LED284 — BINARY_CLOSE

**Physical context:** CH8, channel index 4; CPL (294.640, 161.925) mm;
projected raster (3442.3422, 1649.5803) px.

**Alternative A:** A / STATION / Rockaway Park-Beach 116 St. The point is on
the blue A terminal lane at the shared printed terminal.

**Alternative B:** S / STATION / Rockaway Park-Beach 116 St. The point is on
the gray shuttle terminal lane at the shared printed terminal.

**Nearby confirmed anchors:** The nearest confirmed local A stations are
Beach 67 St (LED276) and Beach 60 St (LED275). LED283 is the paired terminal
footprint; LED281/LED282 at Beach 105 are the preceding S candidate pair.
Both A and S are printed at the terminal.

**Sequence constraints:** S / Beach 116 follows Beach 105 as the shuttle
terminal. The A alternative must align with the separate blue terminal lane.
As with LED283, if both footprint centers correspond to separate route
lanes, the pair should split A/S.

**Human question:** At the Rockaway Park-Beach 116 terminal, is red marker 8
on the blue A terminus or the gray S terminus?

## STATION_VS_SEGMENT — Atlantic / DeKalb corridor

### LED136 — STATION_VS_SEGMENT

**Physical context:** CH4, channel index 22; CPL (76.327, 145.669) mm;
projected raster (992.1111, 1831.9839) px.

**Alternative A:** B / SEGMENT / Atlantic Av-Barclays Ctr -> DeKalb Av. The
projection lies on the B corridor without a distinct B station marker.

**Alternative B:** Q / SEGMENT / Atlantic Av-Barclays Ctr -> DeKalb Av. The
projection lies on the Q corridor without a distinct Q station marker.

**Alternative C:** A B or Q transfer/interchange element at the crossing
rather than a route segment, only if the human sees the physical marker
centered on the interchange geometry rather than a route lane. A B/Q station
at either endpoint is not a clean alternative: those station semantics are
already verified on separate LEDs.

**Nearby confirmed anchors:** On B, DeKalb Av (LED189) and Atlantic
Av-Barclays Ctr (LED160); on Q, DeKalb Av (LED190) and Atlantic
Av-Barclays Ctr (LED163). Other confirmed Atlantic transfer-route LEDs
include Q LED163 and B LED160, plus numbered-route station LEDs in the same
complex. No unique preceding/following station can be selected until the
route is identified.

**Sequence constraints:** For either B or Q, a segment interpretation
between DeKalb and Atlantic preserves the neighboring station sequence.
Calling this a station at either endpoint duplicates an already verified
station on that route. Treating it as a transfer connector requires the
projected point to sit on the connector, not merely near the interchange.
Legacy B/Q review material is secondary and includes a stale LED136/LED157
pairing; LED157 is currently verified as route 2 / Atlantic, so that old
pairing must not be used as a route anchor.

**Human question:** At red marker 1 in the transfer crop, is the LED center
on the B track, Q track, or a connector/station node; and is a printed
station dot centered there?

## MULTIWAY — DeKalb and 7 Av / Prospect Park

### LED191 — MULTIWAY

**Physical context:** CH4, channel index 27; CPL (59.817, 163.957) mm;
projected raster (806.8296, 1626.7213) px.

**Alternative A:** B / SEGMENT / DeKalb Av -> Atlantic Av-Barclays Ctr. The
point follows the B lane between its verified DeKalb and Atlantic stations.

**Alternative B:** Q / SEGMENT / DeKalb Av -> Atlantic Av-Barclays Ctr. The
point follows the Q lane over the same station interval.

**Alternative C:** R / SEGMENT / Jay St-MetroTech -> DeKalb Av, only if the
point is centered on the R lane rather than the B/Q tracks. LED293 is already
a verified R segment for this interval, so this interpretation must be
checked for a distinct physical R segment and not treated as a second
semantic copy by default.

**Nearby confirmed anchors:** B / DeKalb Av LED189 and Q / DeKalb Av LED190;
B / Atlantic Av-Barclays Ctr LED160 and Q / Atlantic Av-Barclays Ctr LED163.
R / Jay St-MetroTech -> DeKalb Av segment LED293 is a nearby verified
companion. The transfer complex also contains confirmed numbered-route
station LEDs, but they do not resolve this LED's route.

**Sequence constraints:** B/Q station assignments at DeKalb would duplicate
LED189/LED190, so they are not retained as station alternatives. The B or Q
segment options preserve the DeKalb-to-Atlantic order. An R segment option
overlaps the already verified LED293 segment; if the two positions represent
one physical R interval, recording both would duplicate the segment. A
transfer connector is possible only if the package center is on the
connector geometry and not a colored route lane.

**Human question:** Is red marker 2 centered on the B line, Q line, R line,
or the transfer connector, and does it coincide with a station dot?

### LED290 — MULTIWAY

**Physical context:** CH3, channel index 26; CPL (107.061, 129.032) mm;
projected raster (1337.0359, 2018.7193) px.

**Alternative A:** Q / SEGMENT / 7 Av -> Prospect Park. Q / 7 Av LED50 and
Q / Prospect Park LED51 are confirmed endpoint anchors.

**Alternative B:** B / SEGMENT / 7 Av -> Prospect Park. B / 7 Av LED230 and
B / Prospect Park LED184 are confirmed endpoint anchors.

**Alternative C:** Another corridor/transfer element at the crossing, only
if the point is visibly on that geometry rather than on either route lane.
A station assignment at 7 Av or Prospect Park would duplicate an existing
verified station on B or Q and is not a clean alternative.

**Nearby confirmed anchors:** Q 7 Av LED50 -> Q Prospect Park LED51; B
7 Av LED230 -> B Prospect Park LED184. S / Prospect Park LED185 is a
confirmed transfer companion at Prospect Park, not an endpoint candidate for
this B/Q interval. The point lies between those station complexes; its
nearest preceding/following station anchors depend on which lane it follows.

**Sequence constraints:** Both B and Q segment alternatives preserve the
same station order, so sequence cannot distinguish them. A B/Q station at
7 Av or Prospect Park would duplicate verified semantics. The Q-side segment
was mentioned in historical corridor review only as a possibility; the
current candidate matrix contains no historical-secondary assignment and
does not establish route or endpoints.

**Human question:** At red marker 3, which printed route track is directly
under the LED center—B, Q, or a transfer/crossing connector?

## Crop files

- `ambiguous_13/ambiguous_transfer_downtown.png` — LED136, LED191, LED290 and
  their B/Q/R and transfer anchors.
- `ambiguous_13/ambiguous_broad_channel.png` — LED269, LED270 and the nearby
  A anchors.
- `ambiguous_13/ambiguous_rockaway_branch.png` — LED277-LED284 and the
  nearby A anchors.
