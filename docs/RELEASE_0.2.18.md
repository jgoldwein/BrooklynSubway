# Brooklyn Map 0.2.18

- Add estimated approach pulses using matching TripUpdate arrival predictions:
  0.5 Hz when distant in time, 1 Hz within a minute, 2 Hz within 30 seconds,
  about 3 Hz within 15 seconds. STOPPED_AT remains steady; missing/stalled
  predictions use a fixed blink. Predicted arrival never becomes a fake stop.
- Show a bounded departure trail at the uniquely resolved previous station:
  2 Hz, then 1 Hz, then 0.5 Hz, clearing within 30 seconds. It uses the last
  detected movement timestamp as an estimate, not measured departure distance.
  Both station indications can coexist; they do not add to train counts.
- Stopped occupancy wins over animation. Route visibility, current/brightness
  caps, POWER toggle, reboot-OFF and existing failure/recovery behavior remain.
- Add a persistent MTA update wait setting on the signed-in status page:
  15-120 seconds, default 15. The old default was 30 seconds. Downloads add to
  the total interval; polling faster can return unchanged upstream data.
- Use bounded prediction storage and per-LED compact timing; no additional raw
  feed buffer or dependency. Live rendering supports up to 25 frames/second.

Validation: host suite, bounded prediction capacity/conflict/platform matching,
rate boundaries, stopped priority, departure expiry, stale clearing, polling
validation/persistence/authentication, captured-feed replay and firmware builds.
Captured cycles produced seven and ten matched ETA station candidates, seven
and nine departure candidates, with no prediction overflow. No extra trains
were counted. Physical pulse timing remains an operator observation step.

These are estimates, not continuously measured train locations. MTA feed
reference: https://www.mta.info/document/134521 . The complete-line diagnostic
ordering improvement remains queued. No board or GPIO changes.
