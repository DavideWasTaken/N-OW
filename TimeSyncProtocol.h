#pragma once

#include <cstdint>

#include "TimerLogic.h"

namespace now_timer {

enum class SyncRole : std::uint8_t { Master, Receiver };

constexpr SyncRole syncRoleForProfile(std::uint8_t profile) {
  return profile == 1 ? SyncRole::Master : SyncRole::Receiver;
}

constexpr std::uint32_t TIME_SYNC_MAGIC = 0x4E4F5731UL;  // "NOW1"
constexpr std::uint16_t TIME_SYNC_VERSION = 1;
constexpr std::uint8_t TIME_SYNC_SOURCE_PROFILE = 1;

struct TimeSyncPacket {
  std::uint32_t magic;
  std::uint16_t version;
  std::uint8_t sourceProfile;
  std::uint8_t reserved;
  std::uint32_t sessionId;
  std::uint32_t sequence;
  std::uint32_t secondsOfDay;
  std::uint32_t checksum;
};

static_assert(sizeof(TimeSyncPacket) == 24,
              "ESP-NOW time sync packet layout must stay stable");

constexpr std::uint32_t rotateLeft(std::uint32_t value, unsigned count) {
  return (value << count) | (value >> (32U - count));
}

constexpr std::uint32_t timeSyncChecksum(const TimeSyncPacket &packet) {
  std::uint32_t value = 0x9E3779B9UL;
  value = rotateLeft(value ^ packet.magic, 5);
  value = rotateLeft(value ^ packet.version, 7);
  value = rotateLeft(value ^ packet.sourceProfile, 11);
  value = rotateLeft(value ^ packet.reserved, 13);
  value = rotateLeft(value ^ packet.sessionId, 17);
  value = rotateLeft(value ^ packet.sequence, 19);
  return rotateLeft(value ^ packet.secondsOfDay, 23);
}

constexpr TimeSyncPacket makeTimeSyncPacket(std::uint32_t sessionId,
                                             std::uint32_t sequence,
                                             std::uint32_t secondsOfDay) {
  TimeSyncPacket packet{TIME_SYNC_MAGIC,
                        TIME_SYNC_VERSION,
                        TIME_SYNC_SOURCE_PROFILE,
                        0,
                        sessionId,
                        sequence,
                        secondsOfDay,
                        0};
  packet.checksum = timeSyncChecksum(packet);
  return packet;
}

constexpr bool isValidTimeSyncPacket(const TimeSyncPacket &packet) {
  return packet.magic == TIME_SYNC_MAGIC &&
         packet.version == TIME_SYNC_VERSION &&
         packet.sourceProfile == TIME_SYNC_SOURCE_PROFILE &&
         packet.reserved == 0 && packet.secondsOfDay < CYCLE_SECONDS &&
         packet.checksum == timeSyncChecksum(packet);
}

constexpr bool sequenceIsNewer(std::uint32_t candidate,
                               std::uint32_t previous) {
  return candidate != previous &&
         static_cast<std::int32_t>(candidate - previous) > 0;
}

class SyncDeduplicator {
 public:
  bool accept(const TimeSyncPacket &packet) {
    if (!isValidTimeSyncPacket(packet)) return false;
    if (!hasPacket_ || packet.sessionId != sessionId_) {
      remember(packet);
      return true;
    }
    if (!sequenceIsNewer(packet.sequence, sequence_)) return false;
    remember(packet);
    return true;
  }

 private:
  void remember(const TimeSyncPacket &packet) {
    hasPacket_ = true;
    sessionId_ = packet.sessionId;
    sequence_ = packet.sequence;
  }

  bool hasPacket_ = false;
  std::uint32_t sessionId_ = 0;
  std::uint32_t sequence_ = 0;
};

}  // namespace now_timer
