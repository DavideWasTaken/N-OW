#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "TimeSyncProtocol.h"

namespace now_timer {

class TimeSyncRadio {
 public:
  static constexpr std::uint8_t CHANNEL = 1;
  static constexpr std::uint8_t REPEAT_COUNT = 20;
  static constexpr std::uint64_t REPEAT_INTERVAL_MS = 100;

  bool begin(SyncRole role) {
    role_ = role;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    if (esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK)
      return false;
    if (esp_now_init() != ESP_OK) return false;

    instance_ = this;
    if (role_ == SyncRole::Receiver) {
      receiveQueue_ = xQueueCreate(1, sizeof(TimeSyncPacket));
      if (receiveQueue_ == nullptr) return false;
      if (esp_now_register_recv_cb(&TimeSyncRadio::receiveCallback) != ESP_OK)
        return false;
    } else {
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, BROADCAST_ADDRESS, sizeof(BROADCAST_ADDRESS));
      peer.channel = CHANNEL;
      peer.ifidx = WIFI_IF_STA;
      peer.encrypt = false;
      const esp_err_t addResult = esp_now_add_peer(&peer);
      if (addResult != ESP_OK && addResult != ESP_ERR_ESPNOW_EXIST)
        return false;
      sessionId_ = esp_random();
      if (sessionId_ == 0) sessionId_ = 1;
    }

    ready_ = true;
    return true;
  }

  bool ready() const { return ready_; }

  void startBroadcast(std::uint32_t secondsOfDay, std::uint64_t nowMs) {
    if (!ready_ || role_ != SyncRole::Master) return;
    ++sequence_;
    packetToSend_ =
        makeTimeSyncPacket(sessionId_, sequence_, secondsOfDay);
    repeatsRemaining_ = REPEAT_COUNT;
    nextSendMs_ = nowMs;
  }

  void update(std::uint64_t nowMs) {
    if (!ready_ || role_ != SyncRole::Master || repeatsRemaining_ == 0 ||
        nowMs < nextSendMs_)
      return;
    esp_now_send(BROADCAST_ADDRESS,
                 reinterpret_cast<const std::uint8_t *>(&packetToSend_),
                 sizeof(packetToSend_));
    --repeatsRemaining_;
    nextSendMs_ = nowMs + REPEAT_INTERVAL_MS;
  }

  bool takeReceived(TimeSyncPacket &packet) {
    if (!ready_ || role_ != SyncRole::Receiver || receiveQueue_ == nullptr)
      return false;
    return xQueueReceive(receiveQueue_, &packet, 0) == pdTRUE;
  }

 private:
  static void receiveCallback(const esp_now_recv_info_t *,
                              const std::uint8_t *data, int length) {
    if (instance_ == nullptr || instance_->receiveQueue_ == nullptr ||
        length != static_cast<int>(sizeof(TimeSyncPacket)))
      return;
    TimeSyncPacket packet;
    memcpy(&packet, data, sizeof(packet));
    if (!instance_->deduplicator_.accept(packet)) return;
    xQueueOverwrite(instance_->receiveQueue_, &packet);
  }

  inline static TimeSyncRadio *instance_ = nullptr;
  inline static constexpr std::uint8_t BROADCAST_ADDRESS[6] = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  SyncRole role_ = SyncRole::Receiver;
  bool ready_ = false;
  QueueHandle_t receiveQueue_ = nullptr;
  SyncDeduplicator deduplicator_;
  std::uint32_t sessionId_ = 0;
  std::uint32_t sequence_ = 0;
  TimeSyncPacket packetToSend_ = {};
  std::uint8_t repeatsRemaining_ = 0;
  std::uint64_t nextSendMs_ = 0;
};

}  // namespace now_timer
