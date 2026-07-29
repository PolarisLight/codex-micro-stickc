// SPDX-License-Identifier: MIT
// Copyright (c) 2026 imliubo

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

#include <array>

struct ThreadLight {
  uint32_t color = 0;
  float brightness = 0.0f;
  String effect = "off";
  float speed = 0.0f;
};

struct LightingSide {
  uint32_t color = 0;
  float brightness = 0.0f;
  String effect = "off";
  float speed = 0.0f;
};

struct CodexMicroState {
  std::array<ThreadLight, 6> threads;
  LightingSide ambient;
  LightingSide keys;
  bool connected = false;
  bool ready = false;
  int disconnectReason = 0;
  uint32_t disconnectCount = 0;
  bool dirty = true;
};

class CodexMicroBle {
 public:
  static constexpr uint16_t kVendorId = 0x303A;
  static constexpr uint16_t kProductId = 0x8360;
  static constexpr uint8_t kReportId = 6;

  void begin();
  void maintain();
  void setBattery(uint8_t percentage, bool charging, int16_t voltageMv = 0);
  void sendKey(const char* key, uint8_t action, int8_t agent = -1);
  void sendJoystick(float angle, float distance);
  bool connected();
  CodexMicroState snapshot();
  bool takeTitleLabels(std::array<String, 6>& labels);

 private:
  class ServerCallbacks;
  class InputCallbacks;
  class OutputCallbacks;
  class TitleCallbacks;

  void onConnected(bool connected, int reason = 0);
  void onOutput(const uint8_t* data, size_t length);
  void onTitleWrite(NimBLECharacteristic* characteristic,
                    const uint8_t* data, size_t length);
  void applyAdvertisingPolicy(uint8_t connectionCount);
  void updateConnectionInfo(NimBLEConnInfo& info);
  void handleRpc(const JsonDocument& request);
  void sendResult(JsonVariantConst id, JsonVariantConst result);
  void sendSuccess(JsonVariantConst id);
  void sendJson(const String& json);
  static void txTaskEntry(void* context);
  void txLoop();
  void clearTxQueue();
  void updateThreadLighting(JsonArrayConst values);
  void updateLightingSide(LightingSide& side, JsonObjectConst value);

  NimBLEHIDDevice* hid_ = nullptr;
  NimBLECharacteristic* input_ = nullptr;
  NimBLECharacteristic* output_ = nullptr;
  NimBLECharacteristic* titleSync_ = nullptr;
  NimBLEAdvertising* advertising_ = nullptr;
  SemaphoreHandle_t stateMutex_ = nullptr;
  QueueHandle_t txQueue_ = nullptr;
  TaskHandle_t txTask_ = nullptr;
  CodexMicroState state_;
  String rpcBuffer_;
  String titleRxBuffer_;
  std::array<String, 6> pendingTitleLabels_;
  bool titleLabelsPending_ = false;
  uint8_t batteryPercentage_ = 100;
  bool charging_ = false;
  int16_t batteryVoltageMv_ = 0;
  uint8_t connectionCount_ = 0;
  int lastDisconnectReason_ = 0;
  uint32_t disconnectCount_ = 0;
  uint32_t lastLinkPulseMs_ = 0;
  uint16_t connInterval_ = 0;
  uint16_t connTimeout_ = 0;
  uint16_t connLatency_ = 0;
  uint16_t connMtu_ = 0;
};
