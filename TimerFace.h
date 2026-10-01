#pragma once
#include "TimerLogic.h"
namespace now_timer {
template<class Canvas> void drawDigit(Canvas &canvas, uint8_t digit, int x, int y, uint16_t red) {
  constexpr uint8_t segments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                   0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  if (digit > 9) return;
  const uint8_t mask = segments[digit];
  if (mask & 0x01) canvas.fillRect(x + 4,  y,      8, 2, red);
  if (mask & 0x02) canvas.fillRect(x + 12, y + 2,  4, 12, red);
  if (mask & 0x04) canvas.fillRect(x + 12, y + 16, 4, 12, red);
  if (mask & 0x08) canvas.fillRect(x + 4,  y + 28, 8, 2, red);
  if (mask & 0x10) canvas.fillRect(x,      y + 16, 4, 12, red);
  if (mask & 0x20) canvas.fillRect(x,      y + 2,  4, 12, red);
  if (mask & 0x40) canvas.fillRect(x + 4,  y + 14, 8, 2, red);
}

// Both firmware and host preview use this face, drawn into a cleared back buffer.
template<class Canvas> void drawTime(Canvas &canvas, Time time, uint16_t red) {
  const uint8_t digits[] = {static_cast<uint8_t>(time.hours / 10),
                           static_cast<uint8_t>(time.hours % 10),
                           static_cast<uint8_t>(time.minutes / 10),
                           static_cast<uint8_t>(time.minutes % 10),
                           static_cast<uint8_t>(time.seconds / 10),
                           static_cast<uint8_t>(time.seconds % 10)};
  constexpr int positions[] = {2, 22, 46, 66, 90, 110};
  for (int i = 0; i < 6; ++i) drawDigit(canvas, digits[i], positions[i], 1, red);
  constexpr int separators[] = {40, 84};
  for (int x : separators) {
    canvas.fillRect(x, 9, 4, 3, red);
    canvas.fillRect(x, 21, 4, 3, red);
  }
}
}
