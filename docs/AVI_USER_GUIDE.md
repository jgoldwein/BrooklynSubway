# Avi's Brooklyn Subway guide

Specially Designed for Avi Yao

Guide for firmware **0.2.80**. Start with the
[quick start](AVI_QUICK_START.md) if you just want to try the map.
For the gift box, [print the one-page sheet](shipping/Brooklyn_Subway_Quick_Start.pdf).

## A few useful words

- **Firmware:** the program running the board.
- **Feed:** a batch of subway updates from the MTA.
- **OLED:** the small screen beside the map.
- **IP address:** the board's address on your home network; use it to open settings.
- **MAC address:** the board's network ID; use it to find the right board in UniFi.

## What the map does

The map gets subway reports over your home Wi-Fi. It uses those reports to
show trains at or between the stations printed on the board.

It updates in batches, so a train's light does not move every second. Two trains
can also share one light. Red is a recent station report; it is not a promise
that the train is still stopped at that exact second.

The web settings let you change the default train colors:

- Yellow: approaching.
- Red: stopped or recently reported at the station.
- Green: leaving.
- Blue: uncertain or last-reported location.

## The three status lights

**SYSTEM** starts red while the board checks itself. Green means those checks
passed. The red stage can be very brief.

**NETWORK** is red while disconnected, amber while connecting or setting the
time, and green when ready. During Wi-Fi setup, follow the OLED instructions;
the map and status lights may be off.

**MTA FEED** starts red. Green means a complete fresh update arrived. Amber
means some data is missing or the map is holding older data.

The map can still show useful trains while MTA FEED is amber. Check the web
page if amber lasts a long time. These colors do not identify weekday or
weekend service.

A short low, middle, or high tone marks SYSTEM, NETWORK, or MTA becoming green.
A steady green light does not keep beeping. Quiet hours and startup mute apply.
The separate dim Avi marker lights when a complete fresh feed is working.

Station lights remain dark until usable train data arrives. Quiet hours, POWER,
or a safety stop can also keep lights off. The status lights do not override
those settings.

## The buttons and knob

### POWER

A short press cycles through both displays on, OLED only, map only, both off,
and back to both on. Hold for **3 seconds** to restart with saved settings.

### BRIGHT and DIM

These work when only one display is on. They adjust that display, with a brief
brightness message and sound. They do nothing when both are on or both are off.
Quiet hours can impose a lower brightness limit than your normal setting.

### MODE: follow one line

Press MODE to enter line focus. Turn the knob to choose a line. The OLED shows
that line's mapped trains and lit station lights. MODE's indicator is amber
while the map lights are on.

Press MODE again to return to your saved lines and the previous OLED screen.
Focus is temporary: it never replaces your saved route settings. You can try
any of the 18 lines, including a line you normally keep hidden.

### TEST

Tap TEST to check all subway lines, one whole line at a time. Each line turns
dim red, green, and blue. Allow about **15 seconds**. Press POWER to cancel.

The web page has slower tests if you want more time to inspect a line. A test
temporarily takes over the lights; it does not represent real trains.

### ROUTE and the rotary knob

Normally, turning the knob changes OLED screens. On the **Lines** screen, press
the knob to browse individual lines, then turn it to choose. Press again to
return to screen browsing. This shows line information on the OLED.

ROUTE is a shortcut to Lines. Press ROUTE again to start or pause automatic line
browsing. If you were in MODE focus, ROUTE leaves focus first.

The knob normally clicks as you turn it. Quiet hours can silence the clicks.

**Important:** a **10-second knob hold opens Wi-Fi setup**. Use a quick press
for ordinary browsing. Holding the knob does not reset all firmware settings.

## The OLED screens

Turn the knob through:

1. **Main:** title, train/light counts, and time.
2. **Activity:** approaching, stopped, leaving, and uncertain counts.
3. **Connection:** Wi-Fi name, signal, feeds, and update age.
4. **Device:** firmware, IP address, uptime, and Wi-Fi MAC address.
5. **Lines:** information for one subway line.
6. **Time & Date:** clock, date, and outside temperature in °F when available.
7. **Brooklyn Weather:** temperature in °F, conditions, and how old the data is.
8. **Happy Bar Mitzvah:** Avi's dedication and scrolling family names.

The clock stays up until you turn away. Other screens follow the return-time
setting. The clock has fixed character positions and steady colons, but its
seconds still update. An alarm clock is a future feature.

Time uses New York time, including daylight saving. After a restart it may say
it is synchronizing. Weather is an estimate for Brooklyn, not a sensor in the
room. It normally refreshes about every 15 minutes. Failed requests retry later;
old or missing data is labeled instead of presented as new.

## Connect to Wi-Fi

1. If setup is needed, join **BrooklynMap-Avi** on your phone or computer.
2. Open **http://192.168.4.1/** in a browser.
3. Create an administrator password if asked. Keep it somewhere safe.
4. Choose your home Wi-Fi and enter its password. Use **Show passwords** to
   check what you typed. The board uses 2.4 GHz Wi-Fi.
5. Once it connects, put your phone or computer back on your home Wi-Fi.

To change Wi-Fi later, hold the knob for 10 seconds to reopen setup. Existing
credentials are kept unless replacement credentials successfully connect.

### Start over or prepare the map for someone else

Firmware 0.2.82 adds **Advanced → Firmware defaults**. Sign in, read the warning,
check the confirmation box, then choose **Erase settings and restart**.

This removes saved Wi-Fi and its password, the administrator password, and all
custom settings. It keeps the installed firmware. Keep power connected until
**BrooklynMap-Avi** appears, then follow the Wi-Fi setup steps above. The new
owner creates a new administrator password during setup.

Automatic update preferences also return to their firmware defaults. After setup,
open **Advanced → Firmware updates** and turn automatic updates on if you want
new signed releases to install automatically. The 10-second knob hold only opens
setup; it does not perform this reset.

If you have two boards, set them up one at a time. They use the same setup
network name. The Device screen's MAC address helps identify each board in UniFi.

## Open settings

On the Device screen, find the board's **IP address**. Type that address into a
browser on the same home network. For example: `http://192.168.1.42/`.
Use the address on your own Device screen, not the example.

Use the IP address shown on the map. The optional `brooklynmap.local` name is
disabled in 0.2.80 to avoid an ESP32 discovery-service crash.
Sign in as administrator when prompted. Use settings on your trusted home
network.

For a simple bedroom setup:

1. Open **Screen â†’ Quiet hours and startup greeting**.
2. Enable quiet hours and choose the start and end times.
3. Choose map lights **Off**, **Dim**, or **Keep normal brightness**.
4. Choose a low OLED brightness and save.

Quiet hours mute beeps. The schedule starts applying after the clock is known;
before time sync, normal brightness and sounds apply. Holding DIM while starting
can mute the startup and automatic readiness cues instead.

## If something looks wrong

- **No map lights:** check the POWER mode and quiet hours first. Then check the
  NETWORK and MTA FEED states. Make sure some routes are enabled in settings and
  MODE is not focusing an empty line.
- **Only a few lights in MODE:** that line may have few represented trains right
  now. Press MODE again to return to your saved lines.
- **NETWORK stays red or amber:** check your home Wi-Fi. Reopen setup if needed.
- **MTA FEED stays amber:** open the web page and check update age and feeds.
  Some MTA reports may be delayed even when Wi-Fi works.
- **Weather unavailable:** leave the board connected. It will retry. This does
  not mean the subway feed has stopped.
- **No sound:** check quiet hours and whether DIM was held during startup.
- **Still stuck:** hold POWER for 3 seconds to restart. If it keeps failing,
  ask for help and share the firmware version and status-page details.

Use the supplied 5 V adapter. Ask an adult before changing wiring or replacing
the power supply.

## Secret extras

Hold **TEST + ROUTE together for 2 seconds**, then release both. Repeat to cycle
through the celebration, Subway Express light chaser, family credits, **Avi Yao
in Morse code**, and anniversary preview. POWER exits. Quiet hours still apply.

The light chaser is just for fun; it is not a real train. The dedication also
has a special September 26 greeting when the normal main screen is available.

Weather data: [Open-Meteo](https://open-meteo.com/), licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).


## Send Joel a support report

1. Open your map's web page and choose **Advanced**, then **Get help**.
2. Press **Make support report** and review the text.
3. Press **Send report to Joel**. Your email app opens a draft addressed to
   **goldwein@gmail.com**. Describe the problem, review the message, then tap Send.
4. If the page asks for the full file, press **Save report file** and attach
   `brooklyn-subway-support.txt` in your email app before sending.

If an email app does not open, use **Copy support report** and paste it into your
usual message to Joel. On some phones you must choose Copy after the text is
selected. The report excludes passwords, sign-in details, Wi-Fi names and
network addresses. Making a report does not change settings or send anything.
If your map is unreachable, send a photo of its screen and describe the problem.


## During a software update

Two gentle notes and an **Update starting** message give about three seconds
of notice before an update. Quiet hours mute the notes. The OLED then shows
the version and download progress. **Do not unplug power** while
it downloads, verifies, restarts, or checks startup. A manual update may ask you
to approve the restart on the web page. An automatic update restarts by itself.
After startup passes its checks, **Update complete** appears for eight seconds.
The normal display then returns. If the OLED was off, it returns to being off.
If you see **Update failed**, open the web update page for details and retry.
These messages apply to web updates; USB programming uses a separate bootloader.

A successful settings save plays two gentle rising notes. Quiet hours mute them.
