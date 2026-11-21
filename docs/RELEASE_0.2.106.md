# Brooklyn Subway 0.2.106

## Name each map in General settings

Open **Advanced > General settings** on the map's web page and sign in.
At the top, enter a **Map name**, then press **Save map name**.
For example, use `Avi's map` or `Living room map`.

This labels the individual PCB on its web page and in its monitoring heartbeat.
The map title, recipient and OLED greeting remain separate personalization fields
on the same page. Existing monitoring labels remain in use until a new map name
is saved. Saving a name needs no monitoring key, does not enable monitoring, and
keeps the Device ID, broker, saved key and monitoring On/Off setting unchanged.
Names can be set even before monitoring is provisioned.

Names accept 1-32 plain English characters without leading or trailing spaces.
They survive restarts and firmware updates. Reset to firmware defaults removes
the name along with other saved settings. Wi-Fi setup still uses BrooklynMap-Avi.

## Home Assistant

The Merion Home and Brooklyn Subway overview summaries now show both LAN and
WAN addresses. The local map's LAN address is a link to its local HTTP web page;
it works while the browser can reach that home network. The remote map's LAN
address is informational. Neither WAN address opens remote access.

A read-only **Map name** diagnostic uses the existing heartbeat, without changing
MQTT topics, identities or permissions. The summaries show the reported name.
The dedicated dashboard has a compact Overview and separate Avi and Local
diagnostics tabs, grouping connection, train data, display, updates, activity,
saved settings and memory. These HA dashboard changes are deployed separately
from firmware and do not require a board restart.

## Validation

All four production/bench, network/non-network firmware targets compiled.
Host checks cover independent name persistence, old-record compatibility,
failed-write retention, malformed/corrupt name fallback, bounds, HTML escaping,
administrator authorization policy and Home Assistant discovery preservation.
Existing LAN web and remote diagnostic regression checks passed. HA summary
templates were evaluated using live reports and the saved dashboards read back.
The new form has not yet been exercised on a physical board. This release
changes no GPIO assignments, LED transport, train representation or supply limits.

Public packages contain no individual Wi-Fi or MQTT credentials. Previously
saved credentials and settings are retained.
