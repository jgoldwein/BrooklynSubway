# Program another Brooklyn production PCB

This local frozen bundle contains the 0.2.49 production firmware and
factory images. It is independent of subsequent source edits and PlatformIO
builds. Target: ESP32-S3-WROOM-1U-N16R8, 16 MB flash / 8 MB OPI PSRAM.
0.2.49 fixes first-boot setup AP startup on boards with no saved credentials.
Do not use the XIAO environment or a .bkota package for the initial USB flash.

1. Complete the new board's usual visual, rail and polarity checks. Leave its
   barrel supply disconnected for initial programming. Connect a USB data cable.
   Identify its COM port by unplugging/reconnecting only that board; close any
   serial monitor using that port. COM5 was the original board, and COM6 appeared
   for the newly connected board, but verify before flashing.
2. In a VS Code/PlatformIO PowerShell terminal at the project folder, run:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\dist\0.2.49\factory\flash.py --port COM6
```

Substitute the verified port if different. The helper verifies frozen binary
hashes, then flashes bootloader, production partitions, OTA selector and app at
PlatformIO's ESP32-S3 offsets. No source is built, and no full-chip erase runs.
A successful run prints hash verification and exits without an error. If unable
to connect, hold the board's BOOT control, tap RESET, release BOOT, check the port
again, and retry. If writing is unreliable, append `--baud 115200`.

3. Join the new board's `BrooklynMap-Avi` setup Wi-Fi and open
   http://192.168.4.1/ . Create its administrator password and connect to the
   desired 2.4 GHz-capable Wi-Fi (for example tsunami2). Do not use the
   original board's administrator credentials automatically; each board stores
   its own setup.
4. Find the NEW board's LAN IP in the serial output or UniFi client list. With
   two boards using the brooklynmap hostname, use their individual IP addresses
   to avoid opening the wrong device. The original board remains 10.1.1.56.
5. Open the new IP and verify firmware 0.2.49, the production target, synchronized
   time and successful MTA feeds. After the board's power checks have passed,
   connect the verified 5 V center-positive supply for physical LED operation.
   Subsequent updates can use the normal signed web-update process.

Optional serial monitor (substitute its port):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COM6 --baud 115200
```

Use Ctrl+C to exit before another USB upload. The frozen bundle is local under
dist/0.2.49/factory and is not an extra GitHub release asset.
