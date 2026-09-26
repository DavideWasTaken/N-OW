#pragma once

#include <stdint.h>

#ifndef NOW_CLOCK_PROFILE
#define NOW_CLOCK_PROFILE 1
#endif

namespace now_timer {

constexpr uint32_t speedForProfile(uint8_t profile) {
  return profile == 1 ? 1U
       : profile == 2 ? 2U
       : profile == 3 ? 8U
       : profile == 4 ? 40U
       : profile == 5 ? 250U
                      : 0U;
}

constexpr bool buttonsEnabledForProfile(uint8_t profile) {
  return profile == 1;
}

constexpr uint8_t CLOCK_PROFILE = static_cast<uint8_t>(NOW_CLOCK_PROFILE);
constexpr uint32_t SPEED_MULTIPLIER = speedForProfile(CLOCK_PROFILE);
constexpr bool BUTTONS_ENABLED = buttonsEnabledForProfile(CLOCK_PROFILE);

static_assert(SPEED_MULTIPLIER != 0,
              "NOW_CLOCK_PROFILE must be an integer from 1 through 5");
static_assert(1000000UL % SPEED_MULTIPLIER == 0,
              "Clock speed must divide one real second exactly");

}  // namespace now_timer
