# Brooklyn Subway 0.2.89

Adds **Sounds and what they mean** to the device's **Help** page, with a direct
**Read the sound guide** link and a local explanation of intermittent readiness
beeps. A short high tone can occur when the MTA feed recovers from delayed or
incomplete data and its status light turns green again. It is not an error code
or necessarily a reboot. The online guide now explains this case explicitly.

Beep behavior is unchanged. Web rendering checks and both network builds pass.
Installing the signed update preserves settings. Keep power connected during
installation and use **brooklyn-s3-network** for the production PCB.
