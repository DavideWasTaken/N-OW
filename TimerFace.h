#pragma once
#include "TimerLogic.h"
namespace now_timer {
template<class Canvas> void drawDigit(Canvas &canvas, uint8_t digit, int x, int y, uint16_t red) {
  constexpr uint8_t segments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                   0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  if (digit > 9) return;
  const uint8_t mask = segments[digit];
  if (mask & 0x01) canvas.fillRect(x + 2, y,      4, 2, red);
  if (mask & 0x02) canvas.fillRect(x + 6, y + 2,  2, 12, red);
  if (mask & 0x04) canvas.fillRect(x + 6, y + 16, 2, 12, red);
  if (mask & 0x08) canvas.fillRect(x + 2, y + 28, 4, 2, red);
  if (mask & 0x10) canvas.fillRect(x,     y + 16, 2, 12, red);
  if (mask & 0x20) canvas.fillRect(x,     y + 2,  2, 12, red);
  if (mask & 0x40) canvas.fillRect(x + 2, y + 14, 4, 2, red);
}

// Both firmware and host preview use this face, drawn into a cleared back buffer.
template<class Canvas> void drawTime(Canvas &canvas, Time time, uint16_t red) {
  const uint8_t digits[] = {static_cast<uint8_t>(time.hours / 10),
                           static_cast<uint8_t>(time.hours % 10),
                           static_cast<uint8_t>(time.minutes / 10),
                           static_cast<uint8_t>(time.minutes % 10),
                           static_cast<uint8_t>(time.seconds / 10),
                           static_cast<uint8_t>(time.seconds % 10)};
  constexpr int positions[] = {1, 11, 23, 33, 45, 55};
  for (int i = 0; i < 6; ++i) drawDigit(canvas, digits[i], positions[i], 17, red);
  constexpr int separators[] = {20, 42};
  for (int x : separators) {
    canvas.fillRect(x, 25, 2, 3, red);
    canvas.fillRect(x, 37, 2, 3, red);
  }
}
}
