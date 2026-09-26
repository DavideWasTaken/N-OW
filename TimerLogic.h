#pragma once
#include <stdint.h>
#include "ClockProfile.h"
namespace now_timer {
struct Time { uint8_t hours, minutes, seconds; };
constexpr uint32_t CYCLE_SECONDS = 24UL * 60 * 60;
inline uint32_t acceleratedSecondsAtSpeed(uint64_t elapsedUs,
                                          uint32_t speedMultiplier) {
  if (speedMultiplier == 0 || 1000000UL % speedMultiplier != 0) return 0;
  const uint64_t microsPerDisplayedSecond = 1000000ULL / speedMultiplier;
  return static_cast<uint32_t>(
      (elapsedUs / microsPerDisplayedSecond) % CYCLE_SECONDS);
}
inline uint32_t acceleratedSeconds(uint64_t elapsedUs) {
  return acceleratedSecondsAtSpeed(elapsedUs, SPEED_MULTIPLIER);
}
inline Time splitTime(uint32_t seconds) {
  seconds %= CYCLE_SECONDS;
  return {static_cast<uint8_t>(seconds / 3600),
          static_cast<uint8_t>((seconds / 60) % 60),
          static_cast<uint8_t>(seconds % 60)};
}
}
