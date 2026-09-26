#pragma once

#include <stdint.h>

#include "TimerLogic.h"

namespace now_timer {

struct DisplayState {
  uint32_t seconds;
  bool visible;
  bool dirty;
  bool enteredSetting;
  bool settingChanged;
  bool confirmed;
};

class TimerController {
 public:
  explicit TimerController(uint32_t speedMultiplier = SPEED_MULTIPLIER)
      : speedMultiplier_(speedMultiplier) {}

  void begin(uint64_t nowUs) {
    baseSeconds_ = 0;
    runStartUs_ = nowUs;
    setting_ = false;
    settingHours_ = 0;
    settingMinutes_ = 0;
    confirmationActive_ = false;
    confirmationStartUs_ = 0;
    lastSeconds_ = 0;
    lastVisible_ = true;
    firstOutput_ = true;
  }

  void synchronize(uint32_t secondsOfDay, uint64_t nowUs) {
    baseSeconds_ = secondsOfDay % CYCLE_SECONDS;
    runStartUs_ = nowUs;
    setting_ = false;
    confirmationActive_ = false;
    firstOutput_ = true;
  }

  DisplayState update(uint64_t nowUs, bool hourIncrement,
                      bool minuteIncrement, bool anyDown,
                      bool bothReleasedEvent) {
    const bool settingChanged = hourIncrement || minuteIncrement;
    bool enteredSetting = false;
    bool confirmed = false;
    if (!setting_ && settingChanged) {
      const uint32_t runningSeconds =
          (baseSeconds_ + acceleratedSecondsAtSpeed(
                              nowUs - runStartUs_, speedMultiplier_)) %
          CYCLE_SECONDS;
      const Time runningTime = splitTime(runningSeconds);
      settingHours_ = runningTime.hours;
      settingMinutes_ = runningTime.minutes;
      setting_ = true;
      enteredSetting = true;
      confirmationActive_ = false;
    }
    if (setting_) {
      if (settingChanged) {
        if (hourIncrement)
          settingHours_ = static_cast<uint8_t>((settingHours_ + 1) % 24);
        if (minuteIncrement)
          settingMinutes_ = static_cast<uint8_t>((settingMinutes_ + 1) % 60);
        confirmationActive_ = false;
      } else if (anyDown) {
        confirmationActive_ = false;
      } else if (bothReleasedEvent) {
        confirmationActive_ = true;
        confirmationStartUs_ = nowUs;
      } else if (confirmationActive_ &&
                 nowUs - confirmationStartUs_ >= 6000000ULL) {
        baseSeconds_ = static_cast<uint32_t>(settingHours_) * 3600UL +
                       static_cast<uint32_t>(settingMinutes_) * 60UL;
        runStartUs_ = nowUs;
        setting_ = false;
        confirmationActive_ = false;
        confirmed = true;
      }
    }
    const uint32_t seconds = setting_
        ? static_cast<uint32_t>(settingHours_) * 3600UL +
              static_cast<uint32_t>(settingMinutes_) * 60UL
        : (baseSeconds_ + acceleratedSecondsAtSpeed(
                              nowUs - runStartUs_, speedMultiplier_)) %
              CYCLE_SECONDS;
    const bool visible = !setting_ || !confirmationActive_ ||
        ((nowUs - confirmationStartUs_) / 500000ULL) % 2 == 0;
    const bool dirty = firstOutput_ || seconds != lastSeconds_ ||
                       visible != lastVisible_ || enteredSetting || confirmed;
    firstOutput_ = false;
    lastSeconds_ = seconds;
    lastVisible_ = visible;
    return {seconds, visible, dirty, enteredSetting, settingChanged, confirmed};
  }

 private:
  uint32_t speedMultiplier_;
  uint32_t baseSeconds_ = 0;
  uint64_t runStartUs_ = 0;
  bool setting_ = false;
  uint8_t settingHours_ = 0;
  uint8_t settingMinutes_ = 0;
  bool confirmationActive_ = false;
  uint64_t confirmationStartUs_ = 0;
  uint32_t lastSeconds_ = 0;
  bool lastVisible_ = true;
  bool firstOutput_ = true;
};

}  // namespace now_timer
