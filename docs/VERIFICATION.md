# Verification record

## Automated host verification

The native C++ suite contains 164 passing checks covering:

- all five speed profiles;
- exact accelerated-second boundaries;
- 24-hour wrap behavior;
- 64-bit monotonic timing;
- hour and minute setting limits;
- button boot gating, debounce, short presses and repeat timing;
- overlapping and simultaneous button input;
- six-second blinking confirmation and cancellation;
- ESP-NOW packet construction, checksum validation and range rejection;
- duplicate, out-of-order and master-restart handling;
- receiver synchronization followed by independent accelerated time;
- 128×32 raster bounds and seven-segment geometry.

Run the suite with:

```powershell
./extras/tests/run-tests.cmd
```

## ESP32-S3 builds

All five profiles compile with Espressif Arduino core 3.3.12 and the HUB75 DMA library. The build options contain the expected `NOW_CLOCK_PROFILE=1` through `5` values, and all complete firmware images have distinct SHA-256 hashes.

Binary inspection confirms that only Profile 1 includes `BUTTONS_READY` and `SYNC_BROADCAST`, while Profiles 2–5 include `BUTTONS_DISABLED` and `SYNC_RECEIVED`.

## Prototype verification

- Two chained P4 64×32 HUB75 panels and the SEENGREAT adapter display one continuous 128×32 timer face.
- The measured P4 color routing produces the intended red output.
- Negative HUB75 clock phase removes the intermittent edge pixels seen with the default phase.
- Profile 1 has been flashed to the current ESP32-S3 and verified after upload.
- UART boot reports Clock 1, 1× speed, buttons enabled, ESP-NOW master role, channel 1, two-panel initialization, the P4 color map and negative clock phase.
- The physical buttons still require final soldering and an on-device input test.
- End-to-end ESP-NOW synchronization requires at least one receiver board and remains pending.

Compiled firmware, upload transcripts, serial logs, local port names and device identifiers are excluded from version control.
