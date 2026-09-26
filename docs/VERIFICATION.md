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
- raster bounds and seven-segment geometry.

Run the suite with:

```powershell
./extras/tests/run-tests.cmd
```

## ESP32-S3 builds

All five profiles compile with Espressif Arduino core 3.3.12 and the HUB75 DMA library. The build options contain the expected `NOW_CLOCK_PROFILE=1` through `5` values, and all complete firmware images have distinct SHA-256 hashes.

Binary inspection confirms that only Profile 1 includes `BUTTONS_READY` and `SYNC_BROADCAST`, while Profiles 2–5 include `BUTTONS_DISABLED` and `SYNC_RECEIVED`.

## Prototype verification

- The 64×64 HUB75 panel and SEENGREAT adapter have displayed the red timer face.
- Profile 1 has been flashed to the current ESP32-S3 and verified after upload.
- UART boot reports Clock 1, 1× speed, buttons enabled, ESP-NOW master role, channel 1 and successful HUB75 initialization.
- The physical buttons still require final soldering and an on-device input test.
- End-to-end ESP-NOW synchronization requires at least one receiver board and remains pending.
- The proposed three-panel-wide display remains unimplemented and untested.

Compiled firmware, upload transcripts, serial logs, local port names and device identifiers are excluded from version control.
