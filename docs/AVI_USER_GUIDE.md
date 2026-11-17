# Avi’s Brooklyn Subway Map — User Guide

Specially Designed for Avi Yao

Guide for firmware **0.2.95**. Start with the
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

## Handling, cleaning and caring for the PCB

The **PCB** is the circuit board that carries the map, lights and electronic
parts. Treat it like an electronic device, even when the lights are off.

### Before handling or moving the board

- **Unplug the barrel power adapter and any USB cable first.** The POWER button
  changes the displays; it does not disconnect electrical power.
- Hold the board by its edges. Avoid touching the small parts, solder joints and
  exposed metal contacts. Keep your hands clean and dry, and avoid handling it
  on carpet where static electricity can build up.
- Lift the board itself, not the OLED, antenna, knob or cables. Do not bend it.
  Unplug cables by holding the connector, not pulling the wire.
- Disconnect power before plugging in, unplugging or adjusting the OLED,
  antenna or other board connections. Ask Joel before changing wiring or parts.

### Power and a safe place to use it

The **USB-C connector is for programming and service, not for powering the map
during normal use**. Use the supplied barrel power adapter to run the map.

- Use the supplied **5 V adapter**. The reviewed supply is **5 V, 3 A,
  center-positive**, with the correct barrel plug. A plug that fits can still
  have the wrong voltage or polarity; do not substitute a 9 V or 12 V adapter.
- Keep the board on its stand or secure mounting supports. Exposed contacts
  must not rest on metal, foil, loose screws or other conductive objects.
- **Keep the PCB out of direct sunlight.** Place the map indoors, away from
  sunny windows and heaters.
- Keep drinks away. Use it in a dry place, away from damp areas. Leave space
  around it; do not cover a powered board with fabric or packing material.
- Keep loose metal objects and pets away from the exposed electronics. Ask an
  adult for help if a connector, cable or mounting part is damaged.

### Antenna

The antenna comes out of the **bottom-left corner of the PCB** and is fastened
to the frame. **Take care not to pull on its cable, loosen its attachment or use
it as a handle.** Keep the cable clear when moving the board, fitting the frame
or packing the map, so it does not get pinched or caught.

Leave the antenna connected and secured in its intended position. If it comes
loose or looks damaged, unplug all power and ask Joel for help before adjusting
it.

### OLED mounting magnet

The round magnet on the **back of the board** holds the OLED case against the
**front of the PCB**. The case contains a matching counter-magnet.

**Please do not move, loosen or remove the magnet on the board.** Its position
keeps the OLED case aligned and supported. Handle the board gently around this
area. If the magnet or case becomes loose, unplug all power and ask Joel for
help rather than trying to reposition it yourself.

### Cleaning

1. Unplug **all** power, including USB.
2. Remove loose dust gently with a clean, soft, dry brush. Wipe the OLED cover
   and printed map gently with a soft, dry microfiber cloth. Do not scrub or
   press on the display or small components.
3. Do not spray cleaner onto the board or use water, alcohol, solvents or
   household cleaners. These can damage the printed artwork, screen cover or
   other materials.

If liquid spills onto it, disconnect power if safe to do so and ask Joel for
help before powering it again. Do not try to dry it with a hair dryer or heater.

### Everyday care and packing

Use a comfortable brightness. Dim or turn off the OLED when it is not needed;
leaving a bright, fixed image on an OLED for long periods can cause uneven wear.

If you notice a burning smell, smoke, unusual heat or a damaged power cable,
disconnect power if safe and stop using the map. Do not keep restarting it.

For storage or shipping, disconnect every cable and protect the unpowered board
in an antistatic bag. Put padding around the bag in a sturdy box, leaving no
pressure on the OLED, knob, antenna connector or components. Keep loose screws
and other metal parts in a separate container.

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

One low beep marks SYSTEM ready, two middle beeps mark NETWORK ready, and
three high beeps mark MTA ready. Each pattern plays only once per boot.
Quiet hours and startup mute apply.
The separate dim Avi marker lights when a complete fresh feed is working.

Station lights remain dark until usable train data arrives. Quiet hours, POWER,
or a safety stop can also keep lights off. The status lights do not override
those settings.

## Sounds and what they mean

The sounds tell you what the map is doing. They are not numbered error codes.

- **Four rising notes, then a lower "ding-dong":** the map is starting up.
  Wait for Wi-Fi and train data; this tune alone does not mean they are ready.
- **One low readiness beep:** SYSTEM has turned green. The board's startup
  checks passed.
- **Two middle readiness beeps:** NETWORK has turned green. The network service
  is ready.
- **Three high readiness beeps:** MTA FEED has turned green. A complete, fresh
  train update arrived. This also serves as the feed-ready sound for the dim
  Avi-location light; there is no extra beep for that light.
- **Two gentle rising notes:** a setting was saved successfully. Several saves
  close together may share one confirmation sound.
- **Two gentle descending notes:** a firmware update is starting. Read the
  OLED message and **do not unplug power**. Wait for **Update complete**.
- **Three descending notes:** service has been unavailable for 60 continuous
  seconds. This optional alert is off by default.
- **Three ascending notes:** service returned after an announced loss. It
  plays only if the loss sound actually started. This alert is also optional.
- **One confirmation beep when setup opens:** BrooklynMap-Avi is ready.
  Follow the OLED instructions, including after holding the knob for 10 seconds.
- **A short tick as you turn the knob:** the map recognized a knob turn.
- **A short tone when you press BRIGHT or DIM:** brightness feedback. A higher
  pitch means a brighter setting. Look at the OLED for the actual level.
  These buttons adjust brightness only when one display is on by itself.
- **One to eight beeps during the full channel test:** the number identifies
  the next LED channel being tested: one beep for channel 1, two for channel 2,
  and so on. It is not a subway line number or an error count.
- **Short and long notes in the Morse-code extra:** the map is spelling
  **Avi Yao** for fun. Other secret extras can also play sounds.

Readiness patterns play once per service per boot, so routine returns to green
stay quiet. Older firmware could repeat a high beep when the MTA feed recovered.
That was a readiness cue, not necessarily a restart.

**Wondering what you just heard?** Open **Overview → Train data, colors & beep
tones**. Its **Last sound** line names the actual sound cue and shows how long
ago it started. It refreshes about every two seconds while the page is visible.
Requests muted or cancelled before playback are not reported as new sounds. This is the latest
cue, not a recording or a full history. **Help → Read the sound guide** opens
this explanation.
Routine knob ticks are omitted from **Last Sound**, so turning the knob does
not replace the explanation of an important cue. The ticks still sound normally.

On OLED **page 1/8**, a meaningful sound can also briefly replace the train-count
line with an icon and a short label: a checkmark for **Saved** or **System ready**,
Wi-Fi arcs for **Connected**, a train for **Feed ready**, a down arrow for
**Service lost**, or an up arrow for **Restored**. The clock stays visible.
The normal count returns four seconds after the cue started. Other OLED pages
stay where you left them; setup, updates, tests and brightness feedback take
priority. Knob ticks do not show an icon or restart the four-second timer.

**Optional service alerts:** Open **Screen → Quiet hours and startup greeting**
and check **Play service lost and restored sounds**. Save settings. These alerts
are off by default, wait until service has worked, and do not repeat during one
outage. A Wi-Fi disconnect counts as unavailable. Train data counts as unavailable
when the last usable update is more than 90 seconds old; it must then remain
unavailable for another 60 seconds before the loss alert. Fresh partial updates
can still count as usable, so one missing feed alone does not trigger a tone.
Setup and quiet hours cancel pending alerts, with no catch-up sounds afterward.

**Want it quiet?** Quiet hours mute the beeps, including manual tests and
secret extras. The quiet-hours schedule applies after the clock is synchronized.
Holding **DIM while powering on or restarting** mutes the startup tune and
automatic readiness cues for that boot; it does not mute every later button,
save, update or optional service-alert sound. You may also hear no cue while another test or recovery
operation is taking priority. A missing sound by itself does not prove a fault.

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

On a fresh setup boot, the OLED shows the Bar Mitzvah greeting for about three
seconds, then displays the setup network and address. Opening setup later with
the 10-second knob hold shows the instructions immediately.

1. If setup is needed, join **BrooklynMap-Avi** on your phone or computer.
2. Open **http://192.168.4.1/** in a browser.
3. Create an administrator password if asked. Keep it somewhere safe. Press
   **Save password and continue to Wi-Fi** to save it and open the Wi-Fi step.
   If a map password is already set, setup goes straight to the Wi-Fi step.
4. Choose your home Wi-Fi and enter its password. Use **Show passwords** to
   check what you typed. The board uses 2.4 GHz Wi-Fi.
5. Once it connects, put your phone or computer back on your home Wi-Fi.

Setup pages include **Read the guide** and **Project on GitHub** links. These
open online help in a new tab. Reconnect your phone to home Wi-Fi first; joining
BrooklynMap-Avi alone does not provide internet access.

To change Wi-Fi later, hold the knob for 10 seconds to reopen setup. Existing
credentials are kept unless replacement credentials successfully connect.
The Wi-Fi step includes **Change the map password** if you need to reset an
existing administrator password while connected to setup Wi-Fi.

### Start over or prepare the map for someone else

Choose **Advanced → Reset to Firmware Defaults**. Sign in, read the warning,
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
in Morse code**, anniversary preview, and a **Rangers synth-horn effect**.
The Rangers effect is the sixth selection: three brief buzzer blasts cycling
the F#3/A#3/C#4 horn chord, with dim red, white and blue flashes. It approximates
the supplied synthesizer reference; it does not play the Slapshot song.
POWER exits. Quiet hours still control sound and map brightness.

The light chaser is just for fun; it is not a real train. The dedication also
has a special September 26 greeting when the normal main screen is available.

Weather data: [Open-Meteo](https://open-meteo.com/), licensed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).


## Send Joel a support report

The top-level **Help** menu includes the online guide, **Project on GitHub**,
**About**, and **Licenses**. You can read the local Help page without internet;
GitHub links and full license documents need internet access.

1. Open your map's web page and choose **Help**, then **Make a support report**.
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


## Personalize your map

1. Open the map's web page and choose **Advanced > Personalization**.
2. Sign in with the administrator password if asked.
3. Edit the names, greeting, date, From line, scrolling names or support contact. The preview shows the greeting lines.
4. Press **Save personalization**. The usual two-note save sound confirms the save, unless quiet hours mute it.

Changes take effect right away and remain after a restart or firmware upgrade. The original Avi greeting is the default. Use plain English letters, numbers and punctuation; each field shows its maximum length. The OLED title displays its first 17 characters. Other greeting lines fit 21 characters, and the final names line scrolls.

Leave optional dedication lines empty to hide them. Leave the celebration date empty to disable the annual greeting. The Morse easter egg spells the recipient name using letters and numbers; other punctuation is skipped. Made by is a display credit and does not change project copyright or licenses.

The support button opens an email draft to the saved support email. Review the report and send it yourself. Nothing is sent automatically. The contact details are visible on your map's local Help page.

The Wi-Fi setup network remains **BrooklynMap-Avi**, matching the printed instructions. Personalization does not change Wi-Fi or administrator passwords. **Reset to firmware defaults** restores the original personalization and erases Wi-Fi and administrator settings, so use that only when you want to start over.
