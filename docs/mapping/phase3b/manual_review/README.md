# Phase 3B CPL manual-review atlas

LED positions come from the manufactured CPL and are projected onto the
original 4096 x 3578 V1.1 artwork with the accepted affine; no registration
refit or historical placement transform was used.

**Phase 3B semantic identification is complete:** 302 / 302 map LEDs are
resolved (276 stations, 26 segments, zero unresolved). The physical audit
covers 306 addressable LEDs: 302 map LEDs and four status LEDs. The maximum
CPL-to-placement residual is 0.000631505 mm, within the accepted 0.05 mm
tolerance. The maximum map-center deviation from the accepted projection is
0.007087525 px.

LED299 is confirmed as the L segment Broadway Junction -> Atlantic Av
(endpoint IDs 83 -> 50), superseding its prior station interpretation as
Bushwick Av-Aberdeen St. LED89 remains the station assignment. The current
306-row projected-LED cache is
[`cpl_projected_leds.csv`](cpl_projected_leds.csv); current semantics are
governed by the authoritative worksheet. The earlier final-13 crops and
candidate files remain for historical review context and are explicitly
resolved, not active candidates. The current queue is empty and no semantic
integrity exception remains; see
[`HUMAN_REVIEW_QUEUE.md`](HUMAN_REVIEW_QUEUE.md).

Prior Bay Ridge, Downtown Brooklyn, Rockaway, and route-cluster reviews remain
available as evidence. Historical overlays may show statuses from before
later confirmations; use the worksheet and completion audit for current
semantics. The master overlay is a static view and does not define status.
