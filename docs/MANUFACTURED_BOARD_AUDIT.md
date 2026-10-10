# Manufactured Board Read-Only Audit

Source: `Brooklyn_Active_Subway_Map_V1.1.eprj2`  
SHA-256: `3ecb90c15d79f027a949c047f9c0a6e2f544706750498f9c7cb94209d61b4bc9`

Key verified facts extracted from the final PCB snapshot:

- U17 = ESP32-S3-WROOM-1U-N16R8.
- U14/U15 = SN74AHCT125DR, four LED channels each.
- 306 XL-1615RGBC-2812B-S addressable RGB LEDs.
- Eight channel counts: 37 / 38 / 37 / 37 / 42 / 37 / 38 / 40.
- GPIO1 controls Q1 -> active-low buffer OE; HIGH enables LED buffers.
- USB-C native USB is GPIO19 D- / GPIO20 D+.
- SW11 = EN/reset, SW14 = GPIO0/boot.
- Front controls: TEST 47, ROUTE 38, DIM 39, POWER 40, MODE 41, BRIGHT 42.
- Rotary encoder/push: 15 / 16 / 17.
- I2C OLED header: GPIO9 SCL, GPIO8 SDA.
- Channel 8 first four LEDs align to SYSTEM / NETWORK / MTA FEED / MODE silkscreen labels.

No writes were made to the manufacturing file during this extraction.
