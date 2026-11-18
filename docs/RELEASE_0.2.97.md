# Brooklyn Subway 0.2.97

## My station alerts

- Choose York St (F), High St (A/C), or Jay St-MetroTech (A/C/F/R) under **Advanced > Personalization > My station alerts**. York St is selected by default. Settings survive restarts and upgrades.
- Open **Advanced > Station alerts** for current and upcoming MTA notices, details, start/end times in New York local time, and the last successful check. Both directions are included; optional line-wide notices are labeled separately.
- Turn the knob to **Station alerts (8/9)** for a short summary. Alerts are silent and do not change train-light colors.
- Notices are checked about every five minutes, after train polling. Upcoming notices cover the next 48 hours. At most six are kept, prioritizing current station notices; the page identifies shortened text and overflow.
- Failed or old checks show **Unavailable**, never an all-clear. No matching notice is not a guarantee of normal service. Read the MTA notice before traveling; a station tag does not by itself mean the station is closed.

The bounded protobuf download uses approximately 1 MiB of external PSRAM, with no large internal-RAM fallback. It shares the serialized network worker with train and weather downloads; train polling has priority. No new dependencies or GPIO assignments. No home address is stored.

Validation: synthetic and captured official MTA protobuf decoding; malformed/truncated/full-dataset checks; saved-preference failures and preservation; HTML escaping and maximum-capacity rendering; administrator endpoint and return-target checks; OLED navigation/runtime; production and bench build targets. COM6 verified on 0.2.97: Fresh 8/8 feeds, OLED frame writes successful, OTA health gate confirmed, and a 461,513-byte official alert download decoded in about 2.1 seconds. York St showed one upcoming notice and no current notices. Approximately 6.79 MB PSRAM remained free after allocation.
