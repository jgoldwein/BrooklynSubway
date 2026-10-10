# B/Q Segment Semantic Review

**Status: review only.** No station assignments or master semantic rows were
changed. The review uses manufactured V1.1 regional artwork, the exact PCB
LED coordinates, current verified B/Q station anchors, and the Phase 3B
station-review ledgers. Electrical chain order is retained only as physical
identity; it is not evidence of route or segment semantics.

## Findings

Eight records were examined: the two existing verified Q segment anchors,
five ambiguous corridor markers, and one excluded B/Q candidate.

- **Existing verified segment anchors:** 2 (`ALREADY_VERIFIED`)
- **Newly proposed B segments:** 0
- **Newly proposed Q segments:** 0
- **Proposed paired B/Q segment groups:** 0
- **Ambiguous candidates:** 5
- **Unresolved records:** 1, LED161, classified as not B/Q

No newly reviewed LED met the evidence threshold for a segment assignment.
In particular, the artwork does not establish a train-position role and
endpoints for the ambiguous markers. No segment is inferred merely because it
falls near the corridor or between station coordinates.

## Existing verified Q segment anchors

Preserved without changes:

- **LED52 — Q, Parkside Av → Church Av**
- **LED113 — Q, Neck Rd → Sheepshead Bay**

These are the only currently supported Q segment rows in this review. There
are no verified or proposed B segments.

## Candidate dispositions

| LED | PCB coordinate (x, y) | Review classification | Status | Finding |
|---|---:|---|---|---|
| LED290 | (4815.0, -2900.0) | Possible Q-side segment | AMBIGUOUS | It lies in the Prospect Park–7 Av corridor and appears closer to the extrapolated Q side, but route membership and endpoints are not established. |
| LED159 | (3775.0, -2085.0) | Possible B side of a pair | AMBIGUOUS | LED159/LED158 appear B/Q-oriented near the Atlantic convergence; route and station-versus-segment role are unresolved. |
| LED158 | (3685.54, -2163.1388) | Possible Q side of a pair | AMBIGUOUS | Same pair; geometry does not establish a Q segment or its endpoints. |
| LED136 | (3605.0, -2245.0) | Possible B side of a pair | AMBIGUOUS | LED136/LED157 appear B/Q-oriented in the converging corridor; their corridor role is unresolved. |
| LED157 | (3520.0, -2320.0) | Possible Q side of a pair | AMBIGUOUS | Same pair; geometry does not establish a Q segment or its endpoints. |
| LED161 | (3365.0, -2330.0) | NOT B/Q | UNRESOLVED | The current manufactured-geometry review places it outside supported B/Q corridor candidates; no route or endpoints are assigned. |

### Resolution questions

- **LED290:** not resolved as a segment. It remains a possible Q-side marker,
  not a B or Q assignment.
- **LED159/LED158:** not resolved as segments. Their likely paired lane
  orientation does not prove they are train positions.
- **LED136/LED157:** not resolved as segments. Their role in the converging
  corridor is not established.

LED161 is the only candidate rejected as not B/Q in this review. It is retained
in the CSV for traceability, but has no route or segment endpoints.

## Corridor coverage and segment sequences

The artwork and coordinate set were reviewed across DeKalb Av, Atlantic
Av-Barclays Ctr, 7 Av, Prospect Park, the southern Brighton-line corridor
through Brighton Beach, and the Q continuation via Ocean Pkwy, W 8 St-NY
Aquarium, and Coney Island-Stillwell Av. Current verified B/Q station LEDs
were treated as route-specific anchors. The scan found no additional
unassigned LED that could be confidently identified as a B/Q train position
between two same-route station anchors.

The station-anchor continuity used for the review was:

- **B:** DeKalb Av → Atlantic Av-Barclays Ctr → 7 Av → Prospect Park →
  Church Av → Newkirk Plaza → Kings Hwy → Sheepshead Bay → Brighton Beach.
- **Q:** DeKalb Av → Atlantic Av-Barclays Ctr → 7 Av → Prospect Park →
  Parkside Av → Church Av → Beverley Rd → Cortelyou Rd → Newkirk Plaza →
  Avenue H → Avenue J → Avenue M → Kings Hwy → Avenue U → Neck Rd →
  Sheepshead Bay → Brighton Beach → Ocean Pkwy → W 8 St-NY Aquarium →
  Coney Island-Stillwell Av.

- **Newly proposed B segment sequence (geographic order):** none.
- **Newly proposed Q segment sequence (geographic order):** none.
- **Existing verified Q segment sequence represented here:** LED52
  (Parkside Av → Church Av); LED113 (Neck Rd → Sheepshead Bay).
- **Proposed paired B/Q segment groups:** none.

The existing Q anchors cover separate corridor intervals; no missing interval
is presumed to require a segment LED. Segment positions are not assigned from
station proximity, route color alone, LED number, or chain order.

## Physical and project boundaries

The segment review does not alter station semantics, the master worksheet, or
firmware. U23 remains the separate Avi-location indicator outside the 306
addressable LEDs. LED291 remains an ordinary addressable map LED.
