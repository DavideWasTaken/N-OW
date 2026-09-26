# Firmware profiles

One source tree produces all five clocks. `NOW_CLOCK_PROFILE` selects the clock at compile time.

| Profile | Speed | Buttons | ESP-NOW | Output directory |
|---:|---:|---|---|---|
| 1 | 1× | Enabled | Master | `build-profiles/clock-1-1x` |
| 2 | 2× | Disabled | Receiver | `build-profiles/clock-2-2x` |
| 3 | 8× | Disabled | Receiver | `build-profiles/clock-3-8x` |
| 4 | 40× | Disabled | Receiver | `build-profiles/clock-4-40x` |
| 5 | 250× | Disabled | Receiver | `build-profiles/clock-5-250x` |

All profiles share the same 24-hour time domain and renderer. Only Profile 1 configures GPIO10 and GPIO11, enters setting mode, and transmits a confirmed time. Profiles 2–5 do not read the button pins; they wait for valid ESP-NOW synchronization packets and then run at their own fixed speeds.

Profile 1 is the default when `NOW_CLOCK_PROFILE` is not defined.

## Generate every firmware image

```powershell
./extras/build-all-clocks.ps1
```

The script writes isolated artifacts and compile logs to `build-profiles/`, plus a manifest containing each complete image size and SHA-256 hash. The entire output directory is ignored by Git.

Before flashing, check the profile number in the output path and verify the UART boot line:

```text
NOW_TIMER_V4 BOOT; CLOCK=<profile>; SPEED=<multiplier>x; WRAP=24h; BUTTONS=<ON|OFF>; SYNC=<MASTER|RECEIVER>
```
