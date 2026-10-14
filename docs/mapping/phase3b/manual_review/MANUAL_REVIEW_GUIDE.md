# Manual review guide

Review the orange unverified designator labels against the printed station
names and route lanes in the CPL-derived artwork crops. Green labels are
already verified; do not reassign them. The full-resolution master and
individual corridor/transfer crops use the same accepted physical projection.

Reply with one line per reviewed LED, for example:

```text
LED148 = C / Broadway Junction / STATION
LED149 = J / Broadway Junction / STATION
LED150 = Z / Broadway Junction / STATION
```

For an identified segment, name its route and endpoints:

```text
LED250 = SEGMENT / A / station1 -> station2
```

If the artwork does not support a confident identification, use:

```text
LED250 = UNKNOWN
```

Do not infer semantics from LED proximity alone when the printed name, route
lane, or route sequence is unclear. Confirmed reviews are recorded in
`manual_review_ledger.csv`; only explicitly authorized human-confirmed
mappings are promoted to the semantic worksheet and station registry.

## Current next batch

Inspect `next_batch_2_5_eastern_brooklyn.png`. It contains 14 unverified LEDs
in the 2/3/4/5 Eastern Brooklyn corridor from President St toward Flatbush Av,
plus five verified anchors. Numbered markers on the native-resolution artwork
match the side roster; orange entries are unverified and green entries are
verified anchors. No assignments have been made for the unverified LEDs.
