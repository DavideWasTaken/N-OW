#pragma once

#include <cstdint>

namespace now_timer {

struct ButtonEvent {
  bool down;
  bool pressed;
  bool released;
  bool increment;
};

struct ButtonPairEvent {
  ButtonEvent hours;
  ButtonEvent minutes;
  bool ready;
  bool anyDown;
  bool bothReleasedEvent;
};

class ButtonInputs {
 public:
  ButtonPairEvent update(bool rawHours, bool rawMinutes, std::uint64_t nowMs) {
    ButtonPairEvent result = emptyEvent();

    if (!ready_) {
      if (rawHours || rawMinutes) {
        bootReleaseStarted_ = false;
      } else if (!bootReleaseStarted_) {
        bootReleaseStarted_ = true;
        bootReleaseSinceMs_ = nowMs;
      } else if (elapsed(nowMs, bootReleaseSinceMs_, debounceMs_)) {
        ready_ = true;
        hours_.raw = false;
        hours_.rawSinceMs = nowMs;
        minutes_.raw = false;
        minutes_.rawSinceMs = nowMs;
      }
      result.ready = ready_;
      return result;
    }

    const bool wasAnyDown = hours_.down || minutes_.down;
    result.hours = updateButton(hours_, rawHours, nowMs);
    result.minutes = updateButton(minutes_, rawMinutes, nowMs);
    result.ready = true;
    result.anyDown = hours_.down || minutes_.down;
    result.bothReleasedEvent = wasAnyDown && !result.anyDown;
    return result;
  }

 private:
  struct ButtonState {
    bool raw = false;
    bool down = false;
    bool fastRepeat = false;
    std::uint64_t rawSinceMs = 0;
    std::uint64_t pressMs = 0;
    std::uint64_t lastIncrementMs = 0;
    std::uint64_t nextIncrementMs = 0;
  };

  static constexpr std::uint64_t debounceMs_ = 30;
  static constexpr std::uint64_t firstRepeatMs_ = 800;
  static constexpr std::uint64_t slowRepeatMs_ = 180;
  static constexpr std::uint64_t fastStartMs_ = 2000;
  static constexpr std::uint64_t fastRepeatMs_ = 90;

  bool ready_ = false;
  bool bootReleaseStarted_ = false;
  std::uint64_t bootReleaseSinceMs_ = 0;
  ButtonState hours_;
  ButtonState minutes_;

  static bool elapsed(std::uint64_t nowMs, std::uint64_t sinceMs,
                      std::uint64_t intervalMs) {
    return nowMs - sinceMs >= intervalMs;
  }

  static bool deadlineReached(std::uint64_t nowMs, std::uint64_t deadlineMs) {
    return nowMs - deadlineMs < (std::uint64_t{1} << 63);
  }

  static std::uint64_t laterDeadline(std::uint64_t firstMs,
                                     std::uint64_t secondMs) {
    return deadlineReached(firstMs, secondMs) ? firstMs : secondMs;
  }

  static ButtonEvent emptyButtonEvent(bool down = false) {
    return ButtonEvent{down, false, false, false};
  }

  static ButtonPairEvent emptyEvent() {
    return ButtonPairEvent{emptyButtonEvent(), emptyButtonEvent(), false, false,
                           false};
  }

  static ButtonEvent updateButton(ButtonState &state, bool raw,
                                  std::uint64_t nowMs) {
    ButtonEvent result = emptyButtonEvent(state.down);
    if (raw != state.raw) {
      state.raw = raw;
      state.rawSinceMs = nowMs;
    } else if (state.raw != state.down &&
               elapsed(nowMs, state.rawSinceMs, debounceMs_)) {
      state.down = state.raw;
      result.down = state.down;
      result.pressed = state.down;
      result.released = !state.down;
      result.increment = state.down;
      if (state.down) {
        state.fastRepeat = false;
        state.pressMs = nowMs;
        state.lastIncrementMs = nowMs;
        state.nextIncrementMs = nowMs + firstRepeatMs_;
      }
    } else if (state.down && !state.fastRepeat &&
               deadlineReached(
                   nowMs,
                   laterDeadline(state.pressMs + fastStartMs_,
                                 state.lastIncrementMs + fastRepeatMs_))) {
      result.increment = true;
      state.fastRepeat = true;
      state.lastIncrementMs = nowMs;
      state.nextIncrementMs = nowMs + fastRepeatMs_;
    } else if (state.down && deadlineReached(nowMs, state.nextIncrementMs)) {
      result.increment = true;
      state.lastIncrementMs = nowMs;
      state.nextIncrementMs =
          nowMs + (state.fastRepeat ? fastRepeatMs_ : slowRepeatMs_);
    }
    return result;
  }
};

}  // namespace now_timer
