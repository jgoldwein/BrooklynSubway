# Shipping Avi's Brooklyn Subway map

Work through these in order. This is the preparation checklist for Joel;
Avi gets [the quick-start guide](AVI_QUICK_START.md).

## 1. First-time setup rehearsal (current step)

The operator confirmed the 0.2.70 phone setup works. Follow-up 0.2.71 adds
a quiet-aware AP-ready beep and a 10-second phone reconnection reminder/delay.
Those two refinements still need physical confirmation.

Use the secondary COM6 board. Keep COM5 as the unchanged reference.
Do not erase credentials on either board just to test this screen.

- Hold the rotary knob for 10 seconds to open setup Wi-Fi.
- Follow the quick-start guide on a phone, using the existing map password.
  Creating a new password is only needed on a board that has none.
- Check the OLED instructions, the phone's no-internet prompt, and network list.
- Connect to home Wi-Fi. Confirm the success page gives this board's address.
- Rejoin home Wi-Fi and open that address in a normal browser; bookmark it.
- Unplug/reconnect power. Confirm Wi-Fi reconnects without repeating setup,
  the OLED clock works, and fresh train data returns.

A later fresh-device rehearsal must also test initial administrator-password
creation, before the shipping board is cleared of the sender's credentials.
Do not share passwords in support reports or screenshots.

## 2. Support report

Implemented in private 0.2.72: Advanced > Get help > Make support report.
Copy, save, or open an email draft to Joel; long reports use a summary plus
manual file attachment. Passwords, sign-in details, SSID and network addresses
are excluded by explicit field allowlists. Test the phone email handoff before
calling this step complete. No message is sent automatically.

## 3. Release and update rehearsal

Publish one tested signed release; verify installation, reboot, displayed
version and automatic-update settings on the shipping board. A USB development
upload does not prove internet updates work.

Completed on COM6 with 0.2.74 on November 15, 2026: operator confirmed the
web update finished; device reports the new version, successful boot-health
confirmation, fresh 8/8 feeds and live map output. COM5 remains 0.2.50 for its
existing baseline test. Automatic unattended installation and forced physical
rollback are separate from this successful manual internet-update rehearsal.

## 4. Stability and recovery

Run the shipping board about 12 hours on its supplied 5 V, 3 A center-positive
adapter. This is a practical pre-shipping check, not proof of long-term reliability.
Test Wi-Fi loss/recovery, power restart, quiet hours, buttons and update
recovery. Record operator restarts separately from unexpected resets.

## 5. Pack and hand over

Identify the actual shipping board. Remove the sender's Wi-Fi and administrator
credentials using the supported reset procedure; verify setup appears after
restart. Retain the tested firmware and intended gift defaults. Include the
adapter, quick-start card, and a link/QR to the published guide. Verify that link
from a phone before packing. Agree with Avi's household about automatic updates
and any future remote diagnostics. Never expose the local web page by router
port forwarding.
