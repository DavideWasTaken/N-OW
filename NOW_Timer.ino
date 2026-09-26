#include <Arduino.h>
#include <esp_timer.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "ButtonInputs.h"
#include "TimerController.h"
#include "TimerLogic.h"
#include "TimerFace.h"
#include "TimeSyncProtocol.h"
#include "TimeSyncRadio.h"

// SEENGREAT RGB Matrix Adapter Board (E) Rev 2.2; one 64x64 HUB75E panel.
// Same wiring and brightness as the verified RGB test.
constexpr uint8_t BRIGHTNESS = 20;
constexpr int OE_PIN = 4;
constexpr int HOURS_BUTTON_PIN = 10;
constexpr int MINUTES_BUTTON_PIN = 11;
HUB75_I2S_CFG::i2s_pins pins = {
  18, 8, 17,       // R1, G1, B1
  16, 1, 15,       // R2, G2, B2
  7, 48, 6, 47, 2, // A, B, C, D, E
  21, 4, 5         // LAT, OE, CLK
};
HUB75_I2S_CFG config(64, 64, 1, pins);
MatrixPanel_I2S_DMA *display = nullptr;
now_timer::ButtonInputs buttons;
now_timer::TimerController timer(now_timer::SPEED_MULTIPLIER);
now_timer::TimeSyncRadio syncRadio;
uint64_t lastReportSecond = UINT64_MAX;
bool buttonsReadyReported = false;

void renderTime(uint32_t seconds, bool visible) {
  // Clear and draw the back buffer before displaying the complete frame.
  display->fillScreen(0);
  if (visible) {
    now_timer::drawTime(*display, now_timer::splitTime(seconds), 0xF800);
  }
  display->flipDMABuffer();
}

void setup() {
  digitalWrite(OE_PIN, HIGH);
  pinMode(OE_PIN, OUTPUT);
  if constexpr (now_timer::BUTTONS_ENABLED) {
    pinMode(HOURS_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MINUTES_BUTTON_PIN, INPUT_PULLUP);
  }
  Serial0.begin(115200); // CH343 UART bridge.
  delay(500);
  Serial0.printf(
      "NOW_TIMER_V4 BOOT; CLOCK=%u; SPEED=%ux; WRAP=24h; BUTTONS=%s; "
      "SYNC=%s\n",
                 static_cast<unsigned>(now_timer::CLOCK_PROFILE),
                 static_cast<unsigned>(now_timer::SPEED_MULTIPLIER),
                 now_timer::BUTTONS_ENABLED ? "ON" : "OFF",
                 now_timer::syncRoleForProfile(now_timer::CLOCK_PROFILE) ==
                         now_timer::SyncRole::Master
                     ? "MASTER"
                     : "RECEIVER");
  config.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  config.double_buff = true;
  display = new MatrixPanel_I2S_DMA(config);
  if (!display->begin()) {
    digitalWrite(OE_PIN, HIGH);
    pinMode(OE_PIN, OUTPUT);
    while (true) {
      Serial0.println("ERROR: HUB75 DMA initialization failed");
      delay(2000);
    }
  }
  display->setBrightness8(BRIGHTNESS);
  display->clearScreen();
  timer.begin(static_cast<uint64_t>(esp_timer_get_time()));
  Serial0.println("HUB75_INIT_OK; DOUBLE_BUFFER=ON; PANEL=64x64; BRIGHTNESS=20");
  const now_timer::SyncRole syncRole =
      now_timer::syncRoleForProfile(now_timer::CLOCK_PROFILE);
  const bool syncReady = syncRadio.begin(syncRole);
  Serial0.printf("ESP_NOW_%s; ROLE=%s; CHANNEL=%u; MAC=%s\n",
                 syncReady ? "READY" : "ERROR",
                 syncRole == now_timer::SyncRole::Master ? "MASTER"
                                                        : "RECEIVER",
                 static_cast<unsigned>(now_timer::TimeSyncRadio::CHANNEL),
                 WiFi.macAddress().c_str());
  if constexpr (!now_timer::BUTTONS_ENABLED) {
    Serial0.println("BUTTONS_DISABLED; CLOCK_PROFILE_HAS_NO_CONTROLS");
  }
}

void loop() {
  const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
  const uint64_t nowMs = nowUs / 1000ULL;
  now_timer::TimeSyncPacket receivedPacket;
  if constexpr (now_timer::CLOCK_PROFILE != 1) {
    if (syncRadio.takeReceived(receivedPacket)) {
      timer.synchronize(receivedPacket.secondsOfDay, nowUs);
      const now_timer::Time received =
          now_timer::splitTime(receivedPacket.secondsOfDay);
      Serial0.printf(
          "SYNC_RECEIVED %02u:%02u:%02u; SESSION=%08lX; SEQUENCE=%lu\n",
          static_cast<unsigned>(received.hours),
          static_cast<unsigned>(received.minutes),
          static_cast<unsigned>(received.seconds),
          static_cast<unsigned long>(receivedPacket.sessionId),
          static_cast<unsigned long>(receivedPacket.sequence));
    }
  }
  now_timer::ButtonPairEvent buttonEvents = {};
  if constexpr (now_timer::BUTTONS_ENABLED) {
    const bool hoursDown = digitalRead(HOURS_BUTTON_PIN) == LOW;
    const bool minutesDown = digitalRead(MINUTES_BUTTON_PIN) == LOW;
    buttonEvents = buttons.update(hoursDown, minutesDown, nowMs);
    if (buttonEvents.ready && !buttonsReadyReported) {
      Serial0.println(
          "BUTTONS_READY; HOURS=GPIO10; MINUTES=GPIO11; ACTIVE=LOW");
      buttonsReadyReported = true;
    }
  }

  const now_timer::DisplayState state =
      timer.update(nowUs, buttonEvents.hours.increment,
                   buttonEvents.minutes.increment, buttonEvents.anyDown,
                   buttonEvents.bothReleasedEvent);
  if (state.dirty) {
    renderTime(state.seconds, state.visible);
  }

  if (state.settingChanged) {
    const now_timer::Time setting = now_timer::splitTime(state.seconds);
    Serial0.printf("SETTING %02u:%02u:00\n",
                   static_cast<unsigned>(setting.hours),
                   static_cast<unsigned>(setting.minutes));
  }
  if (state.confirmed) {
    const now_timer::Time confirmed = now_timer::splitTime(state.seconds);
    Serial0.printf("SET_CONFIRMED %02u:%02u:00\n",
                   static_cast<unsigned>(confirmed.hours),
                   static_cast<unsigned>(confirmed.minutes));
    if constexpr (now_timer::CLOCK_PROFILE == 1) {
      syncRadio.startBroadcast(state.seconds, nowMs);
      Serial0.printf("SYNC_BROADCAST %02u:%02u:00; REPEATS=%u\n",
                     static_cast<unsigned>(confirmed.hours),
                     static_cast<unsigned>(confirmed.minutes),
                     static_cast<unsigned>(
                         now_timer::TimeSyncRadio::REPEAT_COUNT));
    }
  }

  syncRadio.update(nowMs);

  const uint64_t realSecond = nowUs / 1000000ULL;
  if (realSecond != lastReportSecond) {
    const now_timer::Time time = now_timer::splitTime(state.seconds);
    Serial0.printf(
        "TIMER real_ms=%llu HH:MM:SS=%02u:%02u:%02u CLOCK=%u SPEED=%ux\n",
                   static_cast<unsigned long long>(nowMs),
                   static_cast<unsigned>(time.hours),
                   static_cast<unsigned>(time.minutes),
                   static_cast<unsigned>(time.seconds),
                   static_cast<unsigned>(now_timer::CLOCK_PROFILE),
                   static_cast<unsigned>(now_timer::SPEED_MULTIPLIER));
    lastReportSecond = realSecond;
  }
  delay(1);
}
