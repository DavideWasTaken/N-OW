# N:OW technical design

## Purpose

N:OW drives five red digital clocks for an art installation created for [matteomandelli.com](https://matteomandelli.com/). All five clocks begin from one manually selected time and then separate into five temporal rates: 1×, 2×, 8×, 40× and 250×.

## Rendering

The current target is two chained P4 64×32 HUB75 panels per clock, producing one 128×32 canvas. Six custom seven-segment digits occupy 124 pixels horizontally and 30 pixels vertically. The renderer draws into a cleared back buffer and flips complete DMA frames, avoiding partially drawn times.

The tested 1200-nit modules use a panel-specific RGB data order and require the negative HUB75 clock phase to avoid intermittent edge pixels. Both settings live in `NOW_Timer.ino` and apply identically to all five clock profiles.

The clock value is derived from monotonic microseconds rather than frame count. A slow frame may skip intermediate displayed seconds, especially at 250×, but it does not accumulate timing error.

## Profiles

`ClockProfile.h` maps profile numbers 1–5 to their speed multipliers. The selected profile is fixed at compile time so every physical clock has an identifiable firmware image. Profile 1 is the default and the only profile with active controls.

## Master controls

Clock 1 uses GPIO10 for hours and GPIO11 for minutes. Both pins use `INPUT_PULLUP`; each normally-open button connects its GPIO to the shared ground when pressed.

`ButtonInputs` provides independent 30 ms debounce and non-blocking repeat timing. `TimerController` owns running, setting and confirmation state. Any setting input resets seconds to zero. When both buttons are released, the selected `HH:MM:00` flashes every 500 ms for 6 seconds. Further input cancels that confirmation window.

## ESP-NOW start synchronization

All clocks use ESP-NOW broadcast on Wi-Fi channel 1. No access point or Wi-Fi credentials are used.

At confirmation, Clock 1 emits 20 copies of a 24-byte packet over approximately 2 seconds. The packet contains:

- protocol magic and version;
- source profile;
- random master boot-session identifier;
- monotonic confirmation sequence;
- seconds since midnight;
- payload checksum.

Receivers validate the full packet, reject values outside a 24-hour day, ignore duplicates and out-of-order sequences, and accept a new session after a master reboot. Applying a packet resets the receiver's monotonic origin and cancels any local setting state. It then continues independently at its own compile-time speed.

## Failure behavior

Radio initialization failure is reported over UART but does not stop the local display or timer. A receiver that was not powered during the master's confirmation does not receive that starting time; confirm the master again while every clock is powered.

## Current scope

The firmware is configured for 64×32 modules with chain length 2. The prototype validates the complete 128×32 render path, color routing, clock phase and separate 5 V delivery to both panels. Physical button operation and multi-board ESP-NOW synchronization remain the outstanding installation tests.
