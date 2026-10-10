# Logical Subway Map Model

## Identity layers

The model keeps train/feed state, logical map identity, and physical LED
identity separate:

1. A future feed adapter produces a `TrainPosition` with route, trip, direction,
   station-or-segment location, and timestamps.
2. The logical map matches that position to a `MapElementId`.
3. Each logical element carries a separate `PhysicalLedRef`.
4. The display helper writes the physical channel/index into the existing
   framebuffer; the existing backend controls rendering.

`PhysicalLedRef` retains channel, channel index, global physical index, and PCB
designator as separate values. A global physical index is never treated as a
station or map-element identifier.

## Stations, segments, and shared stations

Logical map elements can be `Station` or `Segment`. A station element has its
own `StationId`; a segment records `fromStation`, `toStation`, and optional
direction. The type supports a distinct element for each route at a shared
station. Two route-specific station elements must have different physical LED
references. Segments are also route-specific and may represent train
positions between stations. Interpolation is not implemented.

For example, the model can distinguish Brighton Beach on B from Brighton Beach
on Q, and can keep route-specific Broadway Junction LEDs separate. The current
repository does not establish the route-to-LED assignments for those examples,
so no such mapping is populated or implied.

## Current mapping and unknowns

The physical inventory is derived from the existing
`data/led_chain_map.csv`/`led_map` topology. The first four CH8 indices are the
SYSTEM, NETWORK, MTA FEED, and MODE status LEDs and are excluded. Each of the
remaining 302 physical map LEDs appears as a deterministic logical inventory
entry with a separate `MapElementId` and valid physical reference.

The discrete Avi-location indicator U23 on GPIO14 (`USER_LED`, active HIGH) is
not an addressable LED and is outside this inventory. LED291 remains an
ordinary addressable map element at CH5 index 18 (global physical index 167).
The validator checks that LED291 remains among the 302 unknown map entries.

Until a route, station, or segment identity is verified, the entry is explicitly
`Unknown`: route and station IDs are zero, direction is `Unknown`, and
`assigned` is false. At present there are 302 unknown/unassigned entries, zero
assigned station or segment elements, and zero represented routes. This is
intentional; the chain CSV documents electrical order/designators, not subway
cartography or transit semantics. Do not infer route or station identities
from LED numbering, PCB designators, chain order, or visual assumptions alone.

When validated cartographic source material becomes available, add each
route-specific station or segment assignment to the logical inventory with a
stable ID and its physical reference, then run the `M` validation on XIAO. A
station served by multiple routes requires separate assigned elements and
physical references. Keep the source and rationale for each assignment
auditable alongside the mapping update.

## Validation and bench command

The validator checks the exact eight-channel/306-LED topology, physical
reference bounds and identity, duplicate assignments, exclusion of CH8 status
indices 0-3, and the route-specific shared-station invariant. It also checks
that assigned station/segment elements have the IDs and physical references
they require, while unassigned entries have explicit unknown values. A
synthetic invariant fixture verifies that separate routes at the same station
can use distinct LEDs and that a duplicate physical assignment is rejected; it
does not claim those fixture identities are real subway data.

On XIAO, send `M` or `m` in the serial monitor to run validation, print
inventory counts, and show three concise logical-to-physical examples. The
existing `T`/`t` LED-engine self-test command and heartbeat remain unchanged.
The model does not fetch GTFS or GTFS-Realtime data and does not arm or transmit
production LEDs.
