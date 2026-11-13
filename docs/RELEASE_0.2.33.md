# Brooklyn Map 0.2.33

- Remove arrival/departure blinking. Movement is represented by continuous
  color: approaching orange to cyan during the final 30 predicted seconds;
  departing blue to magenta while fading continuously over at most 30 seconds.
- Missing/stalled approach predictions remain steady orange. Do not invent
  progress when timing is uncertain. Former blink phases no longer gate map
  visibility, including in logical rendering and replay.
- Preserve the configurable blue hold (20-second default), steady dim amber
  uncertainty, shared-stop priority, route visibility, brightness/current caps,
  POWER behavior, OLED dimmer/address handling and authorization fixes.
- The compatibility `blinkingLeds` live statistic now reports zero for traffic.
  Status indicators and deliberate diagnostic sequences are unchanged.

Validation: regression samples cover former on/off phase boundaries for arrivals,
unknown ETA and departures, continuous departure visibility until expiry, shared
stop priority, live worker updates during network stalls, and train accounting.
The complete host suite and four firmware targets are checked before publication.
