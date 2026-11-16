# Brooklyn Subway 0.2.87

Adds **Help** to the normal device website's top-level menu. It includes:

- A visible **Project on GitHub** link and the online user guide.
- **Make a support report**, moved from Advanced.
- **About**, including the firmware version and device type.
- **Licenses**, with original project terms and links to full third-party notices.

Help is public and readable on the local network without administrator sign-in.
Online guide and license links need internet and open in a new tab. On phones,
the six menu items use two rows. Updated assets bypass older browser caches.
The support page returns to Help. The printable quick-start guide has also
been updated to match the new menu.

Validation: 949 LAN page checks and 167 administrator checks pass. Production
and bench network builds pass. Maximum escaped home page remains within the
existing buffer, with more room than before. The one-page PDF was regenerated,
its QR code decoded and its rendered layout inspected. Physical browser
appearance on the device has not yet been confirmed for this release.

Installing the signed update preserves settings. Use **brooklyn-s3-network**
for the production PCB. Keep power connected during installation.
