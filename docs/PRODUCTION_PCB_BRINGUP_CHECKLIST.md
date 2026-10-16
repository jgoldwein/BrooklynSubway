# Production PCB bring-up checklist

## Read before connecting power

**PRODUCTION PCB SCHEMATIC AUDIT:** GO for USB-only initial bring-up with the
external 5-V/barrel supply disconnected.

**PRODUCTION PCB PHYSICAL VALIDATION:** NOT YET PERFORMED.

**PHYSICAL LED OUTPUT:** NOT AUTHORIZED by this checklist.

This checklist does not authorize testing or energizing the external LED
power rail. Keep external 5 V/barrel power disconnected throughout this
initial bring-up. Stop after the final output-disabled check below.

The current-estimation model, 4,000 mA budget, and 16/255 production
brightness cap have not been validated against measured current or the
manufactured production PCB.

## Initial USB-only sequence

Perform these steps only when the manufactured PCB is available and the
USB-only bring-up is separately scheduled. Keep physical LED output disabled
(`BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=0`) and do not enable LED_ENABLE.

1. Visually inspect the manufactured PCB for assembly damage, solder bridges,
   reversed parts, debris, and connector damage. Do not power a board with an
   unexplained defect.
2. Confirm the external 5-V/barrel supply is disconnected.
3. Connect USB only. Do not connect an external supply or LED-rail source.
4. Confirm stable USB enumeration by the host.
5. Open the serial monitor and capture boot diagnostics.
6. Record the reported reset reason.
7. Verify flash and PSRAM detection/capacity against the production
   ESP32-S3-WROOM-1U-N16R8 target.
8. Confirm the feed buffer is allocated in external RAM.
9. Verify Wi-Fi association using stored credentials or the approved
   provisioning flow.
10. Verify the credential state without printing or recording passwords.
11. Verify SNTP synchronization.
12. Verify TLS connection setup.
13. Retrieve and decode one MTA feed.
14. Retrieve and decode all configured feeds.
15. Verify the TrainPipeline completes a cycle.
16. Verify OperationalDisplay behavior in memory only.
17. Verify Phase 4N output-bridge publication in memory only.
18. Observe memory and task stability during the approved soak period.
19. Verify physical LED output remains disabled and no LEDs are illuminated.

**STOP after step 19.** Connecting external 5 V to the LED rail is a later,
separately authorized physical-LED phase, not part of USB-only bring-up.

## Stop for review before any physical-LED action

Obtain explicit review and authorization before:

- connecting external 5 V to the LED rail
- enabling LED_ENABLE
- setting `BROOKLYN_ALLOW_PHYSICAL_LED_OUTPUT=1`
- transmitting WS2812 data
- changing power/current limits
- changing the production brightness cap

## Immediate stop conditions

Disconnect USB when safe and stop for review if any of these occur:

- unexpected heating or unexpected LED illumination
- USB enumeration instability
- boot loops
- PSRAM failure
- brownout or unexplained reset
- unexplained current draw
- networking causes a reset or watchdog event
- mapping or self-check failure

Do not proceed by changing pins, LED mapping, power limits, or output
authorization to work around a stop condition.

## Current readiness state

- Phase 4N output bridge is a memory-only firmware path; it does not authorize
  physical LED transmission.
- Production PCB physical validation has not been performed.
- External 5-V/barrel power and physical LEDs remain prohibited until a later
  physical-LED phase is explicitly authorized.
- The next safe hardware action, when the production PCB is available, is the
  staged USB-only sequence above.