# Brooklyn Subway 0.2.85

Adds **Read the guide** and **Project on GitHub** links to the captive setup
pages, including the successful connection page. Links open a new tab so the
setup page is retained. Before connection, a reminder explains that online help
requires internet access. After connection, it reminds the user to reconnect
their phone to home Wi-Fi before opening online help.

The map joining Wi-Fi does not itself move the phone from BrooklynMap-Avi to
the home network. Existing phone-reconnection and map-opening instructions
remain in place.

Validation: all 439 provisioning checks pass, including page states, link
targets, external-tab protection and maximum escaped network names. Production
and bench network builds pass. Physical captive-browser appearance has not
been rechecked. No settings are changed by installing this signed update.

Keep power connected during installation. Use the **brooklyn-s3-network**
package for the production PCB.
