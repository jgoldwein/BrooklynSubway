# COM6 transient LED flash investigation

## Evidence and limits

The user observed short flashes on lit and previously dark LEDs across the
board. They persisted after substituting a regulated 5 V supply. Turning remote
monitoring off and rebooting reduced their frequency, but did not eliminate
them. That suggests additional load can aggravate the problem; it does not prove
MQTT is its sole cause. Slow-motion video contains transient dark-to-blue events,
including around playback 6.7 seconds. High-speed camera/PWM artifacts must also
be considered when interpreting individual frames.

Release 0.2.103 enabled production MQTT tasks and changed TLS allocation policy.
It did not change the LED backend, NeoPixel version or output refresh policy.
The old backend sent all channels about 25 times per second using NeoPixel
1.12.3's legacy RMT adapter: install a driver, translate bytes in interrupts using
one hardware memory block, wait, then uninstall it for every physical channel.
Its void `show()` API did not report errors to the output safety controller.

Espressif describes [WS2812 frame corruption under Wi-Fi load and IDF 4.x
mitigations](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/rmt.html).
This is a plausible mechanism, not an electrically measured diagnosis.

## Controlled change

`src/led_transport_esp.cpp` uses the already pinned ESP-IDF 4.4 RMT API; no new
package is required. It reserves transmitter 0 and all four S3 TX memory blocks
(192 pulse items) for serialized use by the eight manufactured data pins.
Do not add another RMT user without reviewing this reservation.

- Install once on core 1, away from the Wi-Fi core; use IRAM and level 3.
- Pre-encode up to 42 LEDs into a static 4,036-byte internal-RAM buffer. The ISR
  copies pulse items; it neither translates pixel bytes nor accesses PSRAM.
- Retain GRB order and previous 400/850 ns zero, 800/450 ns one timing at a
  verified 40 MHz counter. Append 300 microseconds LOW to each channel.
- Detach each completed data pin to LOW before routing the next pin.
- Check RMT return values and wait at most 100 ms for completion. On a reported
  failure, immediately force GPIO1 LOW, stop transmission, detach all data pins
  LOW and latch the transport fault until reboot. The existing safety controller
  also disarms. Preserve black-first arming and brightness/current limits.

Affected pins: GPIO1 enable and existing data GPIO4/5/6/7/10/11/12/13. No wiring,
LED ordering, train interpretation, color selection or intentional blink policy
is changed. The driver still depends on refill interrupts; this mitigation is
not DMA and cannot by itself prove that no waveform corruption occurred.

## Validation and physical trial

Run `python tools/run_led_transport_tests.py`,
`python tools/run_led_diagnostic_tests.py`,
`python tools/run_physical_output_safety_tests.py`, and
`python tools/run_live_display_tests.py`.
Transport tests compile the production adapter against fake IDF calls and cover
every pixel, byte and pulse, channel isolation, repeated frames, persistent
installation, invalid inputs, and injected initialization and transmission
failures. They do not measure physical waveforms or interrupt latency.

For the COM6 trial, retain the saved dim brightness and power supply. First watch
for several minutes with monitoring Off, then enable monitoring and repeat.
Check the startup `LED transport: persistent RMT` line, Fresh feeds, responsive
controls, and absence of `LED transport fault` messages. Visual confirmation is
required. If flashes persist, capture LED data with a logic analyzer or test a
DMA transport before claiming the issue fixed.

Keep the verified ordinary 0.2.103 image locally for rollback. Do not publish
0.2.104 as a proven fix before evaluating the physical trial.

## Initial COM6 result, November 20, 2026

The local 0.2.104 build was installed over USB with a verified flash hash.
The user reported no flashes with monitoring Off, then no flashes at all after
turning monitoring On for the requested observation. MQTT resumed with Fresh
8/8 feeds and no reported Wi-Fi/MQTT disconnects. The new LAN and WAN fields
were also received. The result supports the driver mitigation; it does not
establish the exact electrical cause or replace a longer stability test.
