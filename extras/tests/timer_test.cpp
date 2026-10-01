#include "../../ClockProfile.h"
#include "../../TimerLogic.h"
#include "../../TimerController.h"
#include "../../TimerFace.h"
#include "../../ButtonInputs.h"
#include "../../TimeSyncProtocol.h"
#include <stdio.h>
#include <stdlib.h>
static int failures = 0;
static void check(bool ok, const char *name) {
  printf("%s %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++failures;
}
static bool noButtonPulses(const now_timer::ButtonPairEvent &event) {
  return !event.hours.pressed && !event.hours.released && !event.hours.increment &&
         !event.minutes.pressed && !event.minutes.released && !event.minutes.increment &&
         !event.bothReleasedEvent;
}
static void readyAtZero(now_timer::ButtonInputs &buttons) {
  buttons.update(false, false, ~std::uint64_t{0} - 29);
  buttons.update(false, false, 0);
}
static bool quietWhileHoursDown(const now_timer::ButtonPairEvent &event) {
  return event.ready && event.hours.down && noButtonPulses(event);
}
static bool hoursRepeat(const now_timer::ButtonPairEvent &event) {
  return event.ready && event.hours.down && !event.hours.pressed &&
         !event.hours.released && event.hours.increment &&
         !event.minutes.down && !event.minutes.pressed &&
         !event.minutes.released && !event.minutes.increment &&
         !event.bothReleasedEvent;
}
struct Canvas {
  static constexpr int WIDTH = 128;
  static constexpr int HEIGHT = 32;
  bool pixels[HEIGHT][WIDTH] = {};
  bool outOfBounds = false;
  void fillRect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0 || y < 0 || w < 0 || h < 0 || x+w > WIDTH || y+h > HEIGHT) {
      outOfBounds = true; return;
    }
    for (int row=y; row<y+h; ++row)
      for (int col=x; col<x+w; ++col) pixels[row][col] = color != 0;
  }
  int lit() const {
    int n=0; for (const auto &row:pixels) for (bool p:row) n+=p; return n;
  }
  void save(const char *path) const {
    FILE *file = nullptr;
    if (fopen_s(&file,path,"wb") || !file) { ++failures; return; }
    fprintf(file,"P6\n%d %d\n255\n",WIDTH,HEIGHT);
    for (const auto &row:pixels) for (bool p:row) {
      const unsigned char rgb[3] = {static_cast<unsigned char>(p?255:0),0,0};
      fwrite(rgb,1,3,file);
    }
    fclose(file);
  }
};
int main() {
  using namespace now_timer;
  check(syncRoleForProfile(1) == SyncRole::Master &&
            syncRoleForProfile(2) == SyncRole::Receiver &&
            syncRoleForProfile(3) == SyncRole::Receiver &&
            syncRoleForProfile(4) == SyncRole::Receiver &&
            syncRoleForProfile(5) == SyncRole::Receiver,
        "clock 1 broadcasts and clocks 2 through 5 receive synchronization");

  const TimeSyncPacket syncPacket = makeTimeSyncPacket(
      0x12345678UL, 7UL, 12UL * 3600 + 34UL * 60);
  check(isValidTimeSyncPacket(syncPacket) &&
            syncPacket.sessionId == 0x12345678UL &&
            syncPacket.sequence == 7UL &&
            syncPacket.secondsOfDay == 12UL * 3600 + 34UL * 60,
        "time synchronization packet round-trips its session sequence and time");

  TimeSyncPacket corruptSyncPacket = syncPacket;
  corruptSyncPacket.secondsOfDay += 60;
  check(!isValidTimeSyncPacket(corruptSyncPacket),
        "time synchronization packet rejects corrupted payload");
  corruptSyncPacket = syncPacket;
  corruptSyncPacket.secondsOfDay = CYCLE_SECONDS;
  corruptSyncPacket.checksum = timeSyncChecksum(corruptSyncPacket);
  check(!isValidTimeSyncPacket(corruptSyncPacket),
        "time synchronization packet rejects time outside one day");

  SyncDeduplicator syncDeduplicator;
  check(syncDeduplicator.accept(syncPacket),
        "receiver accepts first synchronization packet");
  check(!syncDeduplicator.accept(syncPacket),
        "receiver ignores a repeated packet with the same session and sequence");
  const TimeSyncPacket nextSyncPacket =
      makeTimeSyncPacket(0x12345678UL, 8UL, 13UL * 3600);
  check(syncDeduplicator.accept(nextSyncPacket),
        "receiver accepts the next sequence from the same master session");
  check(!syncDeduplicator.accept(syncPacket),
        "receiver ignores an older packet that arrives out of order");
  const TimeSyncPacket rebootedMasterPacket =
      makeTimeSyncPacket(0x87654321UL, 1UL, 14UL * 3600);
  check(syncDeduplicator.accept(rebootedMasterPacket),
        "receiver accepts a new master session after a reboot");

  check(speedForProfile(1) == 1 && speedForProfile(2) == 2 &&
            speedForProfile(3) == 8 && speedForProfile(4) == 40 &&
            speedForProfile(5) == 250,
        "five clock profiles map to 1x 2x 8x 40x 250x");
  check(CLOCK_PROFILE == 1 && SPEED_MULTIPLIER == 1,
        "default firmware is clock 1 master at 1x");
  check(buttonsEnabledForProfile(1) && !buttonsEnabledForProfile(2) &&
            !buttonsEnabledForProfile(3) && !buttonsEnabledForProfile(4) &&
            !buttonsEnabledForProfile(5),
        "only clock 1 enables hour and minute buttons");
  check(BUTTONS_ENABLED,
        "default clock 1 firmware compiles with buttons enabled");
  const uint32_t profileSpeeds[] = {1, 2, 8, 40, 250};
  for (const uint32_t speed : profileSpeeds) {
    const uint64_t tickUs = 1000000ULL / speed;
    check(acceleratedSecondsAtSpeed(tickUs - 1, speed) == 0 &&
              acceleratedSecondsAtSpeed(tickUs, speed) == 1,
          "profile has exact first simulated-second boundary");
    check(acceleratedSecondsAtSpeed(1000000ULL, speed) == speed,
          "profile advances by its multiplier in one real second");
    check(acceleratedSecondsAtSpeed(CYCLE_SECONDS * tickUs - 1, speed) ==
              CYCLE_SECONDS - 1 &&
              acceleratedSecondsAtSpeed(CYCLE_SECONDS * tickUs, speed) == 0,
          "profile wraps exactly after one displayed day");
  }
  TimerController defaultProfile;
  defaultProfile.begin(0);
  DisplayState defaultDisplay =
      defaultProfile.update(1000000ULL, false, false, false, false);
  check(defaultDisplay.seconds == 1,
        "default controller advances clock 1 by one second per real second");

  TimerController synchronizedReceiver(250);
  synchronizedReceiver.begin(1000000ULL);
  synchronizedReceiver.synchronize(12UL * 3600 + 34UL * 60, 2000000ULL);
  DisplayState synchronizedDisplay = synchronizedReceiver.update(
      2000000ULL, false, false, false, false);
  check(synchronizedDisplay.seconds == 12UL * 3600 + 34UL * 60 &&
            synchronizedDisplay.visible && synchronizedDisplay.dirty,
        "received synchronization immediately starts at HH:MM:00");
  synchronizedDisplay = synchronizedReceiver.update(
      3000000ULL, false, false, false, false);
  check(synchronizedDisplay.seconds ==
            (12UL * 3600 + 34UL * 60 + 250UL) % CYCLE_SECONDS,
        "synchronized receiver continues at its own 250x speed");

  TimerController running(10);
  running.begin(5000000ULL);
  DisplayState display = running.update(5000000ULL, false, false, false, false);
  check(display.seconds == 0 && display.visible && display.dirty,
        "controller begin starts at 00:00:00 with visible dirty output");
  check(!display.enteredSetting && !display.settingChanged && !display.confirmed,
        "running first output has no setting event pulses");
  display = running.update(5099999ULL, false, false, false, false);
  check(display.seconds == 0 && display.visible && !display.dirty,
        "running stays clean between numeric changes");
  display = running.update(5100000ULL, false, false, false, false);
  check(display.seconds == 1 && display.visible && display.dirty,
        "running advances one displayed second after 100000us");
  display = running.update(6000000ULL, false, false, false, false);
  check(display.seconds == 10 && display.visible && display.dirty,
        "running counts at ten displayed seconds per real second");
  display = running.update(86404999999ULL, false, false, false, false);
  check(display.seconds == 86399 && display.dirty,
        "running reaches the last second before midnight");
  display = running.update(86405000000ULL, false, false, false, false);
  check(display.seconds == 0 && display.visible && display.dirty,
        "running wraps at midnight");

  TimerController enterWithHour(10);
  enterWithHour.begin(0);
  display = enterWithHour.update(4529600000ULL, true, false, true, false);
  check(display.seconds == 13UL * 3600 + 34UL * 60 && display.visible &&
            display.dirty && display.enteredSetting && display.settingChanged &&
            !display.confirmed,
        "first hour increment captures 12:34 and enters setting at 13:34:00");

  TimerController enterWithMinute(10);
  enterWithMinute.begin(0);
  display = enterWithMinute.update(4529600000ULL, false, true, true, false);
  check(display.seconds == 12UL * 3600 + 35UL * 60 && display.enteredSetting &&
            display.settingChanged,
        "first minute increment captures 12:34 and enters setting at 12:35:00");

  TimerController wrapHour(10);
  wrapHour.begin(0);
  display = wrapHour.update((23UL * 3600 + 10UL * 60) * 100000ULL,
                            true, false, true, false);
  check(display.seconds == 10UL * 60 && display.enteredSetting &&
            display.settingChanged,
        "setting hour increment wraps 23 to 00");

  TimerController wrapMinute(10);
  wrapMinute.begin(0);
  display = wrapMinute.update((12UL * 3600 + 59UL * 60) * 100000ULL,
                              false, true, true, false);
  check(display.seconds == 12UL * 3600 && display.enteredSetting &&
            display.settingChanged,
        "setting minute increment wraps 59 to 00 without carrying hour");

  TimerController simultaneousSetting(10);
  simultaneousSetting.begin(0);
  display = simultaneousSetting.update(
      (5UL * 3600 + 58UL * 60 + 45UL) * 100000ULL,
      true, true, true, false);
  check(display.seconds == 6UL * 3600 + 59UL * 60 && display.enteredSetting &&
            display.settingChanged && display.dirty,
        "simultaneous increments enter setting and return one combined state");
  display = simultaneousSetting.update(2152500001ULL, false, false, false, false);
  check(display.seconds == 6UL * 3600 + 59UL * 60 && !display.enteredSetting &&
            !display.settingChanged && !display.confirmed && !display.dirty,
        "setting event flags pulse once and quiet unchanged state is clean");
  display = simultaneousSetting.update(2152500002ULL, false, true, true, false);
  check(display.seconds == 6UL * 3600 && !display.enteredSetting &&
            display.settingChanged && display.dirty,
        "later setting change emits one settingChanged pulse");
  display = simultaneousSetting.update(2152500003ULL, false, false, true, false);
  check(display.seconds == 6UL * 3600 && !display.enteredSetting &&
            !display.settingChanged && !display.confirmed && !display.dirty,
        "later settingChanged pulse clears on the next update");
  simultaneousSetting.begin(9000000000ULL);
  display = simultaneousSetting.update(9000000000ULL, false, false, false, false);
  check(display.seconds == 0 && display.visible && display.dirty &&
            !display.enteredSetting && !display.settingChanged && !display.confirmed,
        "begin during setting resets cleanly to running 00:00:00");

  TimerController blinking(10);
  blinking.begin(1000000ULL);
  blinking.update(1000000ULL, true, false, true, false);
  display = blinking.update(2000000ULL, false, false, false, true);
  check(display.seconds == 3600 && display.visible && !display.dirty &&
            !display.confirmed,
        "setting release starts confirmation visible without a redraw");
  display = blinking.update(2499000ULL, false, false, false, false);
  check(display.visible && !display.dirty && !display.confirmed,
        "confirmation stays visible through release plus 499ms");
  display = blinking.update(2500000ULL, false, false, false, false);
  check(!display.visible && display.dirty && !display.confirmed,
        "confirmation turns off at release plus 500ms");
  display = blinking.update(2999000ULL, false, false, false, false);
  check(!display.visible && !display.dirty && !display.confirmed,
        "confirmation stays off through release plus 999ms");
  display = blinking.update(3000000ULL, false, false, false, false);
  check(display.visible && display.dirty && !display.confirmed,
        "confirmation turns on at release plus 1000ms");
  display = blinking.update(7999000ULL, false, false, false, false);
  check(!display.visible && display.dirty && !display.confirmed,
        "confirmation follows the off phase at release plus 5999ms");
  display = blinking.update(8000000ULL, false, false, false, false);
  check(display.seconds == 3600 && display.visible && display.dirty &&
            display.confirmed && !display.enteredSetting &&
            !display.settingChanged,
        "confirmation completes exactly at release plus 6000ms");
  display = blinking.update(8000001ULL, false, false, false, false);
  check(display.seconds == 3600 && display.visible && !display.dirty &&
            !display.confirmed,
        "confirmed pulse clears and quiet running output stays clean");

  TimerController restartConfirmation(10);
  restartConfirmation.begin(0);
  restartConfirmation.update(0, false, true, true, false);
  restartConfirmation.update(1000, false, false, false, true);
  display = restartConfirmation.update(501000, false, false, false, false);
  check(!display.visible && display.dirty && !display.confirmed,
        "restart test reaches confirmation off phase");
  display = restartConfirmation.update(600000, false, false, true, false);
  check(display.visible && display.dirty && !display.confirmed,
        "any held button forces setting visible and cancels confirmation");
  display = restartConfirmation.update(7000000, false, false, false, false);
  check(display.visible && !display.dirty && !display.confirmed,
        "cancelled confirmation waits quietly for a later release");
  display = restartConfirmation.update(7100000, false, true, true, false);
  check(display.seconds == 120 && display.visible && display.dirty &&
            display.settingChanged && !display.confirmed,
        "new setting change remains visible after cancellation");
  display = restartConfirmation.update(7200000, false, false, false, true);
  check(display.seconds == 120 && display.visible && !display.dirty &&
            !display.settingChanged && !display.confirmed,
        "later release restarts a full confirmation window");
  display = restartConfirmation.update(13199000, false, false, false, false);
  check(!display.visible && display.dirty && !display.confirmed,
        "restarted confirmation is still active at plus 5999ms");
  display = restartConfirmation.update(13200000, false, false, false, false);
  check(display.seconds == 120 && display.visible && display.dirty &&
            display.confirmed,
        "restarted confirmation completes only after the full 6000ms");

  TimerController deadlineInput(10);
  deadlineInput.begin(0);
  deadlineInput.update(0, true, false, true, false);
  deadlineInput.update(1000, false, false, false, true);
  display = deadlineInput.update(6001000, false, true, true, false);
  check(display.seconds == 3660 && display.visible && display.dirty &&
            display.settingChanged && !display.confirmed,
        "increment at the exact confirmation deadline wins over confirmation");
  display = deadlineInput.update(12001000, false, false, false, false);
  check(display.seconds == 3660 && display.visible && !display.dirty &&
            !display.confirmed,
        "deadline input cancels confirmation until another release");

  TimerController lateDay(10);
  lateDay.begin(50);
  for (int hour = 0; hour < 23; ++hour)
    display = lateDay.update(50, true, false, true, false);
  for (int minute = 0; minute < 59; ++minute)
    display = lateDay.update(50, false, true, true, false);
  check(display.seconds == 23UL * 3600 + 59UL * 60 && display.visible &&
            display.settingChanged && !display.confirmed,
        "setting 23:59 immediately after begin is safe");
  lateDay.update(1000, false, false, false, true);
  display = lateDay.update(6001000, false, false, false, false);
  check(display.seconds == 23UL * 3600 + 59UL * 60 && display.visible &&
            display.dirty && display.confirmed,
        "23:59 confirms into running at the selected minute");
  display = lateDay.update(6100999, false, false, false, false);
  check(display.seconds == 23UL * 3600 + 59UL * 60 && !display.dirty,
        "confirmed running time does not tick before 100000us");
  display = lateDay.update(6101000, false, false, false, false);
  check(display.seconds == 23UL * 3600 + 59UL * 60 + 1 && display.dirty,
        "confirmed running time ticks at exactly 100000us");
  display = lateDay.update(11901000, false, false, false, false);
  check(display.seconds == 86399 && display.dirty,
        "confirmed 23:59 reaches 23:59:59");
  display = lateDay.update(12001000, false, false, false, false);
  check(display.seconds == 0 && display.visible && display.dirty,
        "confirmed 23:59 wraps after 23:59:59");

  ButtonInputs boot;
  ButtonPairEvent event = boot.update(false, false, 0);
  check(!event.ready && !event.anyDown && noButtonPulses(event),
        "button pair starts gated while released");
  event = boot.update(false, false, 29);
  check(!event.ready && noButtonPulses(event),
        "boot gate does not open before 30ms");
  event = boot.update(false, false, 30);
  check(event.ready && !event.anyDown && noButtonPulses(event),
        "boot gate opens silently at exactly 30ms");

  ButtonInputs bouncedBoot;
  bouncedBoot.update(false, false, 0);
  bouncedBoot.update(false, false, 29);
  event = bouncedBoot.update(true, false, 30);
  check(!event.ready && noButtonPulses(event),
        "boot bounce resets shared release window");
  event = bouncedBoot.update(false, false, 31);
  check(!event.ready && noButtonPulses(event),
        "shared release window restarts after bounce");
  event = bouncedBoot.update(false, false, 60);
  check(!event.ready, "restarted boot gate remains closed at 29ms");
  event = bouncedBoot.update(false, false, 61);
  check(event.ready && noButtonPulses(event),
        "restarted boot gate opens at 30ms");

  ButtonInputs heldAtBoot;
  heldAtBoot.update(true, false, 0);
  heldAtBoot.update(true, false, 30);
  heldAtBoot.update(true, true, 31);
  event = heldAtBoot.update(true, true, 61);
  check(!event.ready && !event.anyDown && noButtonPulses(event),
        "held boot button suppresses the other button press");
  heldAtBoot.update(true, false, 62);
  event = heldAtBoot.update(true, false, 92);
  check(!event.ready && !event.anyDown && noButtonPulses(event),
        "held boot button suppresses the other button release");
  heldAtBoot.update(false, false, 100);
  event = heldAtBoot.update(false, false, 129);
  check(!event.ready, "held boot gate waits for both released for 30ms");
  event = heldAtBoot.update(false, false, 130);
  check(event.ready && !event.anyDown && noButtonPulses(event),
        "held boot gate eventually opens without events");

  ButtonInputs debounce;
  debounce.update(false, false, 0);
  debounce.update(false, false, 30);
  event = debounce.update(true, false, 40);
  check(!event.hours.down && noButtonPulses(event),
        "button press debounce starts silently");
  event = debounce.update(true, false, 69);
  check(!event.hours.down && noButtonPulses(event),
        "button press is not accepted at 29ms");
  event = debounce.update(true, false, 70);
  check(event.hours.down && event.hours.pressed && event.hours.increment &&
            !event.hours.released && !event.minutes.down,
        "button press and increment fire at 30ms");
  event = debounce.update(true, false, 71);
  check(event.hours.down && noButtonPulses(event),
        "debounced down state persists without pulses");
  event = debounce.update(false, false, 80);
  check(event.hours.down && noButtonPulses(event),
        "button release debounce starts with down sustained");
  event = debounce.update(false, false, 109);
  check(event.hours.down && noButtonPulses(event),
        "button release is not accepted at 29ms");
  event = debounce.update(false, false, 110);
  check(!event.hours.down && !event.hours.pressed && event.hours.released &&
            !event.hours.increment && !event.minutes.down,
        "button release fires at 30ms");

  ButtonInputs slowRepeat;
  readyAtZero(slowRepeat);
  slowRepeat.update(true, false, 0);
  event = slowRepeat.update(true, false, 29);
  check(!event.hours.increment, "repeat test press is quiet at 29ms");
  event = slowRepeat.update(true, false, 30);
  check(event.hours.pressed && event.hours.increment && event.hours.down,
        "validated press increments at P=30ms");
  event = slowRepeat.update(true, false, 829);
  check(quietWhileHoursDown(event), "first repeat is quiet just before P+800");
  event = slowRepeat.update(true, false, 830);
  check(hoursRepeat(event), "first repeat emits at P+800");
  event = slowRepeat.update(true, false, 1009);
  check(quietWhileHoursDown(event), "second repeat is quiet at 179ms");
  event = slowRepeat.update(true, false, 1010);
  check(hoursRepeat(event), "second repeat emits after 180ms");
  event = slowRepeat.update(true, false, 1189);
  check(quietWhileHoursDown(event), "third repeat is quiet at 179ms");
  event = slowRepeat.update(true, false, 1190);
  check(hoursRepeat(event), "third repeat emits at 1190ms");
  event = slowRepeat.update(true, false, 1369);
  check(quietWhileHoursDown(event), "fourth repeat is quiet at 1369ms");
  event = slowRepeat.update(true, false, 1370);
  check(hoursRepeat(event), "fourth repeat emits at 1370ms");
  event = slowRepeat.update(true, false, 1549);
  check(quietWhileHoursDown(event), "fifth repeat is quiet at 1549ms");
  event = slowRepeat.update(true, false, 1550);
  check(hoursRepeat(event), "fifth repeat emits at 1550ms");
  event = slowRepeat.update(true, false, 1729);
  check(quietWhileHoursDown(event), "sixth repeat is quiet at 1729ms");
  event = slowRepeat.update(true, false, 1730);
  check(hoursRepeat(event), "sixth repeat emits at 1730ms");
  event = slowRepeat.update(true, false, 1909);
  check(quietWhileHoursDown(event), "seventh repeat is quiet at 1909ms");
  event = slowRepeat.update(true, false, 1910);
  check(hoursRepeat(event), "seventh repeat emits at 1910ms");
  event = slowRepeat.update(true, false, 2029);
  check(quietWhileHoursDown(event), "fast transition is quiet at 2029ms");
  event = slowRepeat.update(true, false, 2030);
  check(hoursRepeat(event), "fast regime starts at P+2000");
  event = slowRepeat.update(true, false, 2119);
  check(quietWhileHoursDown(event), "fast repeat is quiet at 89ms");
  event = slowRepeat.update(true, false, 2120);
  check(hoursRepeat(event), "fast repeat emits after 90ms");
  event = slowRepeat.update(true, false, 2209);
  check(quietWhileHoursDown(event), "second fast repeat is quiet at 89ms");
  event = slowRepeat.update(true, false, 2210);
  check(hoursRepeat(event), "second fast repeat emits after 90ms");

  ButtonInputs missedFastDeadline;
  readyAtZero(missedFastDeadline);
  missedFastDeadline.update(true, false, 0);
  missedFastDeadline.update(true, false, 30);
  missedFastDeadline.update(true, false, 830);
  missedFastDeadline.update(true, false, 1010);
  missedFastDeadline.update(true, false, 1190);
  missedFastDeadline.update(true, false, 1370);
  missedFastDeadline.update(true, false, 1550);
  missedFastDeadline.update(true, false, 1730);
  missedFastDeadline.update(true, false, 1910);
  event = missedFastDeadline.update(true, false, 2100);
  check(hoursRepeat(event), "missed fast deadline emits exactly once at 2100ms");
  event = missedFastDeadline.update(true, false, 2189);
  check(quietWhileHoursDown(event), "missed fast deadline does not catch up");
  event = missedFastDeadline.update(true, false, 2190);
  check(hoursRepeat(event), "missed fast deadline reschedules from actual emission");

  ButtonInputs delayedSlowRepeat;
  readyAtZero(delayedSlowRepeat);
  delayedSlowRepeat.update(true, false, 0);
  delayedSlowRepeat.update(true, false, 30);
  delayedSlowRepeat.update(true, false, 830);
  delayedSlowRepeat.update(true, false, 1010);
  delayedSlowRepeat.update(true, false, 1190);
  delayedSlowRepeat.update(true, false, 1370);
  delayedSlowRepeat.update(true, false, 1550);
  delayedSlowRepeat.update(true, false, 1730);
  event = delayedSlowRepeat.update(true, false, 1980);
  check(hoursRepeat(event), "delayed slow repeat emits once at 1980ms");
  event = delayedSlowRepeat.update(true, false, 2030);
  check(quietWhileHoursDown(event), "delayed slow repeat defers fast transition at 2030ms");
  event = delayedSlowRepeat.update(true, false, 2069);
  check(quietWhileHoursDown(event), "delayed fast transition is quiet at 2069ms");
  event = delayedSlowRepeat.update(true, false, 2070);
  check(hoursRepeat(event), "delayed fast transition emits at 2070ms");

  ButtonInputs simultaneous;
  readyAtZero(simultaneous);
  simultaneous.update(true, true, 0);
  event = simultaneous.update(true, true, 30);
  check(event.hours.down && event.hours.pressed && event.hours.increment &&
            event.minutes.down && event.minutes.pressed &&
            event.minutes.increment && event.anyDown &&
            !event.bothReleasedEvent,
        "simultaneous presses validate independently");
  simultaneous.update(false, true, 40);
  event = simultaneous.update(false, true, 69);
  check(event.hours.down && event.minutes.down && event.anyDown &&
            !event.bothReleasedEvent,
        "anyDown stays set during release debounce");
  event = simultaneous.update(false, true, 70);
  check(event.hours.released && !event.hours.down && event.minutes.down &&
            event.anyDown && !event.bothReleasedEvent,
        "releasing one button is not a both-released event");
  simultaneous.update(false, false, 80);
  event = simultaneous.update(false, false, 109);
  check(event.minutes.down && event.anyDown && !event.bothReleasedEvent,
        "anyDown stays set until the last release validates");
  event = simultaneous.update(false, false, 110);
  check(event.minutes.released && !event.hours.down && !event.minutes.down &&
            !event.anyDown && event.bothReleasedEvent,
        "last validated release emits bothReleasedEvent once");
  event = simultaneous.update(false, false, 111);
  check(!event.anyDown && !event.bothReleasedEvent,
        "bothReleasedEvent clears on the next update");

  ButtonInputs overlapping;
  readyAtZero(overlapping);
  overlapping.update(true, false, 0);
  overlapping.update(true, false, 30);
  overlapping.update(true, true, 100);
  event = overlapping.update(true, true, 130);
  check(event.hours.down && !event.hours.increment && event.minutes.down &&
            event.minutes.pressed && event.minutes.increment && event.anyDown,
        "second button press does not disturb held first button");
  event = overlapping.update(true, true, 830);
  check(event.hours.increment && event.hours.down && event.minutes.down &&
            !event.minutes.increment && event.anyDown,
        "one button repeats independently while the other is held");
  overlapping.update(true, false, 840);
  event = overlapping.update(true, false, 870);
  check(event.hours.down && event.minutes.released && !event.minutes.down &&
            event.anyDown && !event.bothReleasedEvent,
        "tapping button release does not stop held button");
  overlapping.update(true, true, 880);
  event = overlapping.update(true, true, 910);
  check(event.hours.down && !event.hours.increment && event.minutes.pressed &&
            event.minutes.increment && event.minutes.down && event.anyDown,
        "tapped button can press again while first button is held");
  event = overlapping.update(true, true, 1010);
  check(event.hours.increment && !event.minutes.increment &&
            event.hours.down && event.minutes.down && event.anyDown,
        "held button repeat schedule survives another button tap");
  overlapping.update(true, false, 1020);
  event = overlapping.update(true, false, 1050);
  check(event.hours.down && event.minutes.released && event.anyDown &&
            !event.bothReleasedEvent,
        "pair remains down after tapped button releases");
  overlapping.update(false, false, 1060);
  event = overlapping.update(false, false, 1090);
  check(event.hours.released && !event.anyDown && event.bothReleasedEvent,
        "held button release completes overlapping pair cycle");

  ButtonInputs endToEndButtons;
  TimerController endToEndController(10);
  endToEndController.begin(0);
  unsigned int settingChangedCount = 0;
  unsigned int confirmedCount = 0;
  auto feedEndToEnd = [&](bool rawHours, bool rawMinutes,
                          std::uint64_t nowMs) {
    event = endToEndButtons.update(rawHours, rawMinutes, nowMs);
    display = endToEndController.update(
        nowMs * 1000ULL, event.hours.increment, event.minutes.increment,
        event.anyDown, event.bothReleasedEvent);
    if (display.settingChanged) ++settingChangedCount;
    if (display.confirmed) ++confirmedCount;
  };

  feedEndToEnd(false, false, 0);
  check(!event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 0 && !display.enteredSetting &&
            !display.settingChanged && !display.confirmed,
        "end-to-end boot starts gated without button or controller events");
  feedEndToEnd(false, false, 30);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 0 && !display.enteredSetting &&
            !display.settingChanged && !display.confirmed,
        "end-to-end boot arms silently after both buttons stay released");

  feedEndToEnd(true, false, 100);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            !display.settingChanged,
        "end-to-end hour press waits for debounce");
  feedEndToEnd(true, false, 130);
  check(event.ready && event.hours.down && event.hours.pressed &&
            event.hours.increment && event.anyDown &&
            !event.bothReleasedEvent && display.seconds == 3600 &&
            display.enteredSetting && display.settingChanged &&
            !display.confirmed,
        "end-to-end validated hour press displays 01:00:00");

  feedEndToEnd(false, false, 150);
  check(event.ready && event.anyDown && noButtonPulses(event) &&
            display.seconds == 3600 && !display.settingChanged &&
            !display.confirmed,
        "end-to-end hour release debounce keeps the pair down");
  feedEndToEnd(false, false, 180);
  check(event.hours.released && !event.anyDown && event.bothReleasedEvent &&
            display.seconds == 3600 && !display.settingChanged &&
            !display.confirmed,
        "end-to-end last release starts confirmation");

  feedEndToEnd(false, true, 200);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 3600 && !display.settingChanged &&
            !display.confirmed,
        "end-to-end minute press waits while confirmation is pending");
  feedEndToEnd(false, true, 230);
  check(event.minutes.down && event.minutes.pressed &&
            event.minutes.increment && event.anyDown &&
            !event.bothReleasedEvent && display.seconds == 3660 &&
            display.visible && !display.enteredSetting &&
            display.settingChanged && !display.confirmed,
        "end-to-end minute press displays 01:01:00 and cancels confirmation");

  feedEndToEnd(false, false, 250);
  check(event.anyDown && noButtonPulses(event) && display.seconds == 3660 &&
            display.visible && !display.settingChanged && !display.confirmed,
        "end-to-end held minute keeps confirmation canceled during release debounce");
  feedEndToEnd(false, false, 280);
  check(event.minutes.released && !event.anyDown && event.bothReleasedEvent &&
            display.seconds == 3660 && display.visible &&
            !display.settingChanged && !display.confirmed,
        "end-to-end minute release starts a fresh confirmation window");

  feedEndToEnd(false, false, 780);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 3660 && !display.visible && display.dirty &&
            !display.enteredSetting && !display.settingChanged &&
            !display.confirmed,
        "end-to-end fresh confirmation turns off at release plus 500ms");
  feedEndToEnd(false, false, 1280);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 3660 && display.visible && display.dirty &&
            !display.enteredSetting && !display.settingChanged &&
            !display.confirmed,
        "end-to-end fresh confirmation turns on at release plus 1000ms");

  feedEndToEnd(false, false, 6279);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 3660 && !display.settingChanged &&
            !display.confirmed,
        "end-to-end remains in setting before the fresh deadline");
  feedEndToEnd(false, false, 6280);
  check(event.ready && !event.anyDown && noButtonPulses(event) &&
            display.seconds == 3660 && display.visible &&
            !display.settingChanged && display.confirmed,
        "end-to-end confirms 01:01:00 exactly six seconds after release");
  feedEndToEnd(false, false, 6281);
  check(display.seconds == 3660 && display.visible &&
            !display.settingChanged && !display.confirmed,
        "end-to-end controller pulses clear immediately after confirmation");
  feedEndToEnd(false, false, 7280);
  check(display.seconds == 3670 && display.visible &&
            !display.settingChanged && !display.confirmed,
        "end-to-end running clock reaches 01:01:10 one real second later");
  check(settingChangedCount == 2 && confirmedCount == 1,
        "end-to-end emits exactly two setting changes and one confirmation");

  check(acceleratedSecondsAtSpeed(0,10)==0,"starts at zero");
  check(acceleratedSecondsAtSpeed(99999,10)==0,"no premature second");
  check(acceleratedSecondsAtSpeed(100000,10)==1,"one simulated second per 100ms");
  check(acceleratedSecondsAtSpeed(1000000,10)==10,"10x after one real second");
  check(acceleratedSecondsAtSpeed(5999999,10)==59 && acceleratedSecondsAtSpeed(6000000,10)==60,"minute carry");
  check(acceleratedSecondsAtSpeed(359999999,10)==3599 && acceleratedSecondsAtSpeed(360000000,10)==3600,"hour carry");
  check(acceleratedSecondsAtSpeed(8639999999ULL,10)==86399,"last second before 24-hour wrap");
  check(acceleratedSecondsAtSpeed(8640000000ULL,10)==0,"wrap at 24 hours");
  check(acceleratedSecondsAtSpeed(4294967296000ULL,10)==8872,"no 32bit millis rollover");
  const Time a=splitTime(59), b=splitTime(60), c=splitTime(3599), d=splitTime(3600), e=splitTime(86399);
  check(a.hours==0 && a.minutes==0 && a.seconds==59 && b.minutes==1 && b.seconds==0,"MM:SS boundaries");
  check(c.hours==0 && c.minutes==59 && c.seconds==59 && d.hours==1 && d.minutes==0 && d.seconds==0,"HH:MM boundaries");
  check(e.hours==23 && e.minutes==59 && e.seconds==59,"23:59:59 formatting");
  Canvas all;
  drawTime(all,{88,88,88},0xF800);
  check(!all.outOfBounds && all.lit()>1000,"visible face fits 128x32");
  check(all.pixels[3][2] && all.pixels[3][125],"uses 124 pixels of width");
  check(all.pixels[9][40] && all.pixels[21][84],"two visible colon separators");
  Canvas zero,one;
  drawTime(zero,{0,0,0},0xF800); drawTime(one,{11,11,11},0xF800);
  check(zero.lit()>one.lit() && one.lit()>0,"distinct digit shapes");
  bool bounded=true;
  for(int digit=0;digit<10;++digit) {
    Canvas raster;
    drawTime(raster,{static_cast<uint8_t>(digit*11),static_cast<uint8_t>(digit*11),static_cast<uint8_t>(digit*11)},0xF800);
    bounded &= !raster.outOfBounds;
  }
  check(bounded,"all ten digit shapes stay in bounds");
  Canvas preview;
  drawTime(preview,{12,34,56},0xF800); preview.save("timer-preview.ppm");
  zero.save("timer-zero.ppm");
  printf("%d failures\n",failures);
  return failures ? 1 : 0;
}
