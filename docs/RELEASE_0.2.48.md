# Brooklyn Map 0.2.48

- Remove the fixed 20-second stopped-color cutoff. A supported stop now stays
  red while fresh, consistent evidence continues to support the same trip and
  station. The configured palette still applies if stopped is a different color.
- Keep initial qualification strict: a new supported stop needs a matching
  recent arrival. Once published, the stop can continue with its original older
  arrival only while vehicle, matching prediction and feed remain fresh and
  non-future. Original vehicle/prediction/feed timestamps determine expiry;
  a new feed header cannot refresh frozen inner timestamps.
- Contradictory, missing, future, conflicted or expired evidence breaks
  continuation. A changed trip/station cannot borrow another stop's support.
  Verified departures still take precedence immediately. Unpublished held
  partial candidates cannot establish or renew displayed-stop support.
- Replace the old stop-time form with a plain-language explanation. The retired
  /config/blue-hold POST returns an authenticated notice and changes nothing.
  Stored legacy values remain for rollback but no longer control the display.
  Status APIs expose stoppedPolicy=fresh-evidence; status/monitor also expose
  stoppedTimeLimitEnabled=false. Legacy blueHoldSeconds is an inactive value.

This follows the five-minute 0.2.47 observation: all 180 stopped-light endings
were the fixed cutoff, typically before the next poll arrived. This change does
not turn STOPPED_AT alone into proof of a stop and does not guarantee physical
station dwell. Stale or contradictory evidence becomes uncertain according to
existing color/visibility preferences. No brightness, GPIO, OLED, polling,
partial-map policy, dependency, TLS or power-cap changes.

Validation: all 30 host suites pass (status tests now use the existing production
6144-byte JSON capacity), four firmware environments build, and browser checks
pass at 320/390/768/1100 pixels. New regressions cover sustained fresh stops past
20 seconds, old arrivals requiring prior unexpired confirmation, frozen vehicle
and prediction timestamps, missing/contradictory/future reports, trip/station
identity, conflicts, publication delay, held candidates, outages, rollback and
verified departure precedence. Existing renderer/output and monitoring tests pass.
Production static RAM remains 196596 bytes; no added allocation or flash writes.

After updating, the existing indication collector can verify that red endings
reflect new evidence or source expiry rather than the retired visit-time limit.
