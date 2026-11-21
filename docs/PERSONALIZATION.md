# Make the map your own

## Name this individual PCB

From firmware **0.2.106**, open **Advanced > General settings** and sign in.
At the top, enter **Map name**, then press **Save map name**. This identifies
the individual map, independently of its title, recipient and greeting below.
Use up to 32 plain English characters, with no spaces at the beginning or end.

**You do not need to re-enter the monitoring key.** The name has its own Save
button and can be set before monitoring is configured. The Device ID, key,
server and monitoring On/Off setting stay as they are. When monitoring is
enabled, the new name reaches Home Assistant with the next delivered heartbeat,
normally about 30 seconds later. Network pauses can delay it.

Upgrading preserves each existing monitoring label until you save a new name.
The name survives restarts and upgrades; Reset to firmware defaults removes it.
The setup Wi-Fi network remains **BrooklynMap-Avi**.


Open the map's web page, then **Advanced > General settings**. Sign in, edit the fields, and press **Save personalization**. Changes apply immediately and survive restarts and firmware updates.

- **Map title:** shown on the website and OLED main screen.
- **Recipient and short OLED name:** the person using the map; also used by personalized extras and Morse code.
- **Greeting:** any short welcome or celebration message.
- **From line and scrolling names:** your own dedication or credits. Leave optional lines blank to hide them.
- **Celebration date:** optional; leave blank to disable the annual greeting.
- **Made by:** an editable display credit. It does not change original authorship or license notices.
- **Support contact name:** is called **The Developer** in these generic guides. Replace the saved name with the person supporting your map. Help and report buttons use this saved name.
- **Support email:** optional. The original Avi profile includes its support contact; replace it with the correct address for your map to enable the email-draft button. Copy and Save report work without an email address. Nothing is sent automatically.

The greeting preview shows how the text will fit on the OLED. Field limits are shown on the form. Use plain letters, numbers and punctuation.

## Personal extras

The separate **Personalized extras** form lets you enable Celebration, Anniversary, Family Credits and the Rangers extra, and edit their messages. The original Rangers extra remains enabled by default. Its sound and colors are specifically Rangers even if you change the caption. Existing saved choices are retained. Subway Express and Morse remain available.

## Station alerts

The **My station alerts** form chooses York St, High St or Jay St-MetroTech notices and optional line-wide notices. These are silent; details appear under **Advanced > Station alerts** and on the OLED.

## Existing maps and firmware defaults

A firmware update preserves saved personalization, including the original gift's names and contact details. New/reset maps retain the original Avi greeting, names and support contact. Change these fields for another recipient; saved personalization is kept across upgrades.

**Reset to firmware defaults** removes saved personalization along with Wi-Fi, administrator credentials and other settings. It is not needed to change a name.

The setup network remains **BrooklynMap-Avi**, at **192.168.4.1**, to match the manufactured board and printed recovery instructions. It is a fixed connection name, not the saved recipient name.

The repository guides use generic labels and cannot read settings from your map. The build story, actual photographs, historical release notes, copyright and licenses retain the original names and credits.
