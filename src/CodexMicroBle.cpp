// SPDX-License-Identifier: MIT
// Copyright (c) 2026 imliubo

#include "CodexMicroBle.h"

#include <NimBLEDevice.h>
#include <esp_system.h>

#include "BleAdvertisingPolicy.h"
#include "BleConnectionState.h"

namespace {

constexpr char kDeviceName[] = "Codex Micro";
constexpr char kManufacturer[] = "Work Louder";
constexpr char kFirmwareVersion[] = "0.8.0-stickc-power-soc";
constexpr size_t kPayloadSize = 61;
constexpr size_t kReportBodySize = 63;
constexpr size_t kTxQueueDepth = 12;
constexpr size_t kTitlePayloadLimit = 4096;
constexpr char kTitleServiceUuid[] = "5f83a25b-442d-4d56-bf3a-3e2f8b21e101";
constexpr char kTitleCharacteristicUuid[] =
    "5f83a25b-442d-4d56-bf3a-3e2f8b21e102";
// Windows negotiates a 30 ms connection interval with the StickC.
// Send at most one HID fragment per connection event.
constexpr TickType_t kReportPacing = pdMS_TO_TICKS(35);

// One vendor-defined input/output report. HIDAPI adds/removes Report ID 6,
// while the BLE characteristics carry the remaining 63-byte report body.
const uint8_t kReportMap[] = {
    0x06, 0x00, 0xFF,        // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,              // Usage (1)
    0xA1, 0x01,              // Collection (Application)
    0x85, 0x06,              // Report ID (6)
    0x15, 0x00,              // Logical Minimum (0)
    0x26, 0xFF, 0x00,        // Logical Maximum (255)
    0x75, 0x08,              // Report Size (8)
    0x95, 0x3F,              // Report Count (63)
    0x09, 0x01,              // Usage (1)
    0x81, 0x02,              // Input (Data, Variable, Absolute)
    0x95, 0x3F,              // Report Count (63)
    0x09, 0x02,              // Usage (2)
    0x91, 0x02,              // Output (Data, Variable, Absolute)
    0xC0                     // End Collection
};

}  // namespace

class CodexMicroBle::ServerCallbacks final : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacks(CodexMicroBle& owner) : owner_(owner) {}

  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    owner_.updateConnectionInfo(info);
    owner_.onConnected(true);
  }

  void onConnParamsUpdate(NimBLEConnInfo& info) override {
    owner_.updateConnectionInfo(info);
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int reason) override {
    Serial.printf("BLE disconnect reason=%d handle=%u uptime_ms=%lu\n", reason,
                  info.getConnHandle(), static_cast<unsigned long>(millis()));
    owner_.onConnected(false, reason);
  }

  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    Serial.printf("BLE pairing %s\n", info.isEncrypted() ? "complete" : "failed");
  }

 private:
  CodexMicroBle& owner_;
};

class CodexMicroBle::OutputCallbacks final : public NimBLECharacteristicCallbacks {
 public:
  explicit OutputCallbacks(CodexMicroBle& owner) : owner_(owner) {}

  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
    const NimBLEAttValue value = characteristic->getValue();
    owner_.onOutput(value.data(), value.length());
  }

 private:
  CodexMicroBle& owner_;
};

class CodexMicroBle::InputCallbacks final : public NimBLECharacteristicCallbacks {
 public:
  void onStatus(NimBLECharacteristic*, int code) override {
    if (code != 0) {
      Serial.printf("BLE notify code=%d\n", code);
    }
  }
};

class CodexMicroBle::TitleCallbacks final
    : public NimBLECharacteristicCallbacks {
 public:
  explicit TitleCallbacks(CodexMicroBle& owner) : owner_(owner) {}

  void onWrite(NimBLECharacteristic* characteristic,
               NimBLEConnInfo&) override {
    const NimBLEAttValue value = characteristic->getValue();
    owner_.onTitleWrite(characteristic, value.data(), value.length());
  }

 private:
  CodexMicroBle& owner_;
};

void CodexMicroBle::begin() {
  stateMutex_ = xSemaphoreCreateMutex();
  txQueue_ = xQueueCreate(kTxQueueDepth, sizeof(String*));
  xTaskCreatePinnedToCore(txTaskEntry, "codex-hid-tx", 4096, this, 1, &txTask_,
                          1);

  NimBLEDevice::init(kDeviceName);
  NimBLEDevice::setMTU(67);
  NimBLEDevice::setPower(9);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityAuth(true, false, true);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks(*this));

  hid_ = new NimBLEHIDDevice(server);
  hid_->setManufacturer(kManufacturer);
  hid_->setPnp(0x02, kVendorId, kProductId, 0x0101);
  hid_->setHidInfo(0x00, 0x03);
  hid_->setReportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));

  input_ = hid_->getInputReport(kReportId);
  input_->setCallbacks(new InputCallbacks());
  output_ = hid_->getOutputReport(kReportId);
  output_->setCallbacks(new OutputCallbacks(*this));

  NimBLEService* titleService = server->createService(kTitleServiceUuid);
  titleSync_ = titleService->createCharacteristic(
      kTitleCharacteristicUuid,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC,
      512);
  titleSync_->setCallbacks(new TitleCallbacks(*this));
  titleSync_->setValue("TITLE_SYNC_READY");

  server->start();
  hid_->setBatteryLevel(batteryPercentage_, false);

  advertising_ = NimBLEDevice::getAdvertising();
  advertising_->setAppearance(GENERIC_HID);
  advertising_->setName(kDeviceName);
  advertising_->addServiceUUID(hid_->getHidService()->getUUID());
  advertising_->addServiceUUID(kTitleServiceUuid);
  advertising_->enableScanResponse(true);
  applyAdvertisingPolicy(0);

  Serial.printf(
      "BLE vendor HID ready VID=%04X PID=%04X usage=FF00 report=%u\n",
      kVendorId, kProductId, kReportId);
}

void CodexMicroBle::maintain() {
  // Intentionally quiet. Periodic Battery Service notifications caused the
  // Windows HID link to miss vendor reports and eventually disconnect.
}

void CodexMicroBle::setBattery(uint8_t percentage, bool charging, int16_t voltageMv) {
  batteryPercentage_ = constrain(percentage, 0, 100);
  charging_ = charging;
  batteryVoltageMv_ = voltageMv;
  if (hid_ != nullptr && connected()) {
    hid_->setBatteryLevel(batteryPercentage_, false);
  }
}

void CodexMicroBle::sendKey(const char* key, uint8_t action, int8_t agent) {
  StaticJsonDocument<192> message;
  message["method"] = "v.oai.hid";
  JsonObject params = message.createNestedObject("params");
  params["k"] = key;
  params["act"] = action;


  String json;
  serializeJson(message, json);
  sendJson(json);
  Serial.printf("HID key=%s action=%u\n", key, action);
}

void CodexMicroBle::sendJoystick(float angle, float distance) {
  StaticJsonDocument<160> message;
  message["method"] = "v.oai.rad";
  JsonObject params = message.createNestedObject("params");
  params["a"] = angle;
  params["d"] = distance;

  String json;
  serializeJson(message, json);
  sendJson(json);
}

bool CodexMicroBle::connected() {
  if (stateMutex_ == nullptr) {
    return false;
  }
  xSemaphoreTake(stateMutex_, portMAX_DELAY);
  const bool result = state_.connected;
  xSemaphoreGive(stateMutex_);
  return result;
}

CodexMicroState CodexMicroBle::snapshot() {
  CodexMicroState copy;
  if (stateMutex_ == nullptr) {
    return copy;
  }
  xSemaphoreTake(stateMutex_, portMAX_DELAY);
  copy = state_;
  state_.dirty = false;
  xSemaphoreGive(stateMutex_);
  return copy;
}

bool CodexMicroBle::takeTitleLabels(std::array<String, 6>& labels) {
  if (stateMutex_ == nullptr) {
    return false;
  }
  xSemaphoreTake(stateMutex_, portMAX_DELAY);
  const bool available = titleLabelsPending_;
  if (available) {
    labels = pendingTitleLabels_;
    titleLabelsPending_ = false;
  }
  xSemaphoreGive(stateMutex_);
  return available;
}

void CodexMicroBle::onConnected(bool connected, int reason) {
  xSemaphoreTake(stateMutex_, portMAX_DELAY);
  const bool wasConnected = bleHasConnections(connectionCount_);
  connectionCount_ = connected ? bleConnectionAdded(connectionCount_)
                               : bleConnectionRemoved(connectionCount_);
  state_.connected = bleHasConnections(connectionCount_);
  if (!wasConnected && state_.connected) {
    state_.ready = false;
  }
  if (!state_.connected) {
    state_.ready = false;
    lastDisconnectReason_ = reason;
    ++disconnectCount_;
    state_.disconnectReason = reason;
    state_.disconnectCount = disconnectCount_;
  }
  state_.dirty = true;
  const bool fullyDisconnected = !state_.connected;
  const uint8_t connectionCount = connectionCount_;
  xSemaphoreGive(stateMutex_);
  if (fullyDisconnected) {
    rpcBuffer_.clear();
    clearTxQueue();
  }
  applyAdvertisingPolicy(connectionCount);
  Serial.printf("BLE link %s count=%u\n",
                connected ? "connected" : "disconnected", connectionCount);
}

void CodexMicroBle::applyAdvertisingPolicy(uint8_t connectionCount) {
  if (advertising_ == nullptr) {
    return;
  }
  const BleAdvertisingPolicy policy = advertisingPolicy(connectionCount);
  advertising_->stop();
  advertising_->setAdvertisingInterval(policy.intervalUnits);
  advertising_->start();
  Serial.printf("BLE advertising interval_ms=%u connections=%u\n",
                policy.intervalMs, connectionCount);
}

void CodexMicroBle::updateConnectionInfo(NimBLEConnInfo& info) {
  connInterval_ = info.getConnInterval();
  connTimeout_ = info.getConnTimeout();
  connLatency_ = info.getConnLatency();
  connMtu_ = info.getMTU();
  Serial.printf("BLE params interval=%.2fms latency=%u timeout=%ums mtu=%u\n",
                connInterval_ * 1.25f, connLatency_, connTimeout_ * 10,
                connMtu_);
}
void CodexMicroBle::onOutput(const uint8_t* data, size_t length) {
  if (data == nullptr || length < 2) {
    return;
  }

  // HOGP normally strips the report ID. Accept an included ID as well so the
  // transport remains compatible with hosts that forward the raw report.
  size_t offset = (length >= 3 && data[0] == kReportId) ? 1 : 0;
  if (length < offset + 2 || data[offset] != 2) {
    return;
  }

  const size_t payloadLength = min<size_t>(data[offset + 1], kPayloadSize);
  if (length < offset + 2 + payloadLength) {
    return;
  }
  const char* payload = reinterpret_cast<const char*>(data + offset + 2);
  constexpr char kTopLevelPrefix[] = "{\"method\"";
  const bool startsTopLevel =
      payloadLength >= sizeof(kTopLevelPrefix) - 1 &&
      memcmp(payload, kTopLevelPrefix, sizeof(kTopLevelPrefix) - 1) == 0;
  if (startsTopLevel && !rpcBuffer_.isEmpty()) {
    // A new top-level object means a previous fragmented write was dropped.
    // Resynchronize immediately instead of poisoning the next request.
    rpcBuffer_.clear();
  }
  if (rpcBuffer_.isEmpty()) {
    size_t jsonStart = 0;
    while (jsonStart < payloadLength && payload[jsonStart] != '{') {
      ++jsonStart;
    }
    if (jsonStart == payloadLength) {
      return;
    }
    rpcBuffer_.concat(payload + jsonStart, payloadLength - jsonStart);
  } else {
    rpcBuffer_.concat(payload, payloadLength);
  }

  DynamicJsonDocument request(4096);
  const DeserializationError error = deserializeJson(request, rpcBuffer_);
  if (error == DeserializationError::IncompleteInput) {
    return;
  }
  if (error) {
    Serial.printf("RPC parse error: %s\n", error.c_str());
    rpcBuffer_.clear();
    return;
  }

  handleRpc(request);
  rpcBuffer_.clear();
}

void CodexMicroBle::onTitleWrite(NimBLECharacteristic* characteristic,
                                 const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return;
  }
  if (titleRxBuffer_.length() + length > kTitlePayloadLimit) {
    titleRxBuffer_.clear();
    characteristic->setValue("TITLE_SYNC_ERROR");
    return;
  }
  titleRxBuffer_.concat(reinterpret_cast<const char*>(data), length);
  const int newline = titleRxBuffer_.indexOf('\n');
  if (newline < 0) {
    return;
  }

  const String packet = titleRxBuffer_.substring(0, newline);
  titleRxBuffer_.remove(0, newline + 1);
  DynamicJsonDocument document(3072);
  if (deserializeJson(document, packet) ||
      !document["labels"].is<JsonArray>()) {
    titleRxBuffer_.clear();
    characteristic->setValue("TITLE_SYNC_ERROR");
    return;
  }

  std::array<String, 6> labels;
  JsonArray values = document["labels"].as<JsonArray>();
  for (int i = 0; i < 6; ++i) {
    labels[i] =
        i < static_cast<int>(values.size()) ? values[i].as<String>() : String();
    labels[i].trim();
  }

  xSemaphoreTake(stateMutex_, portMAX_DELAY);
  pendingTitleLabels_ = labels;
  titleLabelsPending_ = true;
  xSemaphoreGive(stateMutex_);
  characteristic->setValue("TITLE_SYNC_OK");
}

void CodexMicroBle::handleRpc(const JsonDocument& request) {
  const char* method = request["method"] | "";
  JsonVariantConst id = request["id"];
  JsonVariantConst params = request["params"];
  Serial.printf("RPC method=%s\n", method);

  if (strcmp(method, "sys.version") == 0) {
    StaticJsonDocument<128> resultDoc;
    resultDoc["version"] = kFirmwareVersion;
    sendResult(id, resultDoc.as<JsonVariantConst>());
    return;
  }

  if (strcmp(method, "device.status") == 0) {
    // Keep the standard heartbeat compact; diagnostics made it five HID
    // fragments and occasionally left the host waiting for a dropped tail.
    StaticJsonDocument<256> resultDoc;
    resultDoc["version"] = kFirmwareVersion;
    resultDoc["profile_index"] = 0;
    resultDoc["layer_index"] = 1;
    resultDoc["battery"] = batteryPercentage_;
    resultDoc["is_charging"] = charging_;
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    state_.ready = true;
    state_.dirty = true;
    xSemaphoreGive(stateMutex_);
    sendResult(id, resultDoc.as<JsonVariantConst>());
    return;
  }

  if (strcmp(method, "v.oai.thstatus") == 0 && params.is<JsonArrayConst>()) {
    updateThreadLighting(params.as<JsonArrayConst>());
    sendSuccess(id);
    return;
  }

  if (strcmp(method, "v.oai.rgbcfg") == 0 && params.is<JsonObjectConst>()) {
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    JsonObjectConst config = params.as<JsonObjectConst>();
    updateLightingSide(state_.ambient, config["ambient"].as<JsonObjectConst>());
    updateLightingSide(state_.keys, config["keys"].as<JsonObjectConst>());
    state_.dirty = true;
    xSemaphoreGive(stateMutex_);
    sendSuccess(id);
    return;
  }

  if (strcmp(method, "lights.preview") == 0 || strcmp(method, "host.focused_app") == 0) {
    sendSuccess(id);
    return;
  }

  StaticJsonDocument<192> response;
  response["id"] = id;
  JsonObject error = response.createNestedObject("error");
  error["code"] = -32601;
  error["message"] = "Method not found";
  String json;
  serializeJson(response, json);
  sendJson(json);
}

void CodexMicroBle::sendResult(JsonVariantConst id, JsonVariantConst result) {
  DynamicJsonDocument response(512);
  response["id"] = id;
  response["result"] = result;
  String json;
  serializeJson(response, json);
  sendJson(json);
}

void CodexMicroBle::sendSuccess(JsonVariantConst id) {
  StaticJsonDocument<96> resultDoc;
  resultDoc["ok"] = true;
  sendResult(id, resultDoc.as<JsonVariantConst>());
}

void CodexMicroBle::sendJson(const String& json) {
  if (input_ == nullptr || txQueue_ == nullptr || !connected()) {
    return;
  }

  String* framed = new String(json);
  if (framed == nullptr) {
    Serial.println("HID TX allocation failed");
    return;
  }
  *framed += '\n';
  if (xQueueSend(txQueue_, &framed, 0) != pdTRUE) {
    Serial.println("HID TX queue full; dropping message");
    delete framed;
  }
}

void CodexMicroBle::txTaskEntry(void* context) {
  static_cast<CodexMicroBle*>(context)->txLoop();
}

void CodexMicroBle::txLoop() {
  for (;;) {
    String* framed = nullptr;
    if (xQueueReceive(txQueue_, &framed, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (framed == nullptr) {
      continue;
    }

    if (connected()) {
      size_t offset = 0;
      while (offset < framed->length() && connected()) {
        const size_t chunk =
            min<size_t>(kPayloadSize, framed->length() - offset);
        uint8_t report[kReportBodySize] = {};
        report[0] = 2;
        report[1] = chunk;
        memcpy(report + 2, framed->c_str() + offset, chunk);
        if (!input_->notify(report, sizeof(report))) {
          Serial.println("BLE notify rejected");
          break;
        }
        offset += chunk;
        vTaskDelay(kReportPacing);
      }
    }
    delete framed;
  }
}

void CodexMicroBle::clearTxQueue() {
  if (txQueue_ == nullptr) {
    return;
  }
  String* queued = nullptr;
  while (xQueueReceive(txQueue_, &queued, 0) == pdTRUE) {
    delete queued;
  }
}

void CodexMicroBle::updateThreadLighting(JsonArrayConst values) {
  xSemaphoreTake(stateMutex_, portMAX_DELAY);
  for (JsonObjectConst value : values) {
    const int id = value["id"] | -1;
    if (id < 0 || id >= static_cast<int>(state_.threads.size())) {
      continue;
    }
    ThreadLight& light = state_.threads[id];
    light.color = value["c"] | light.color;
    light.brightness = value["b"] | light.brightness;
    light.effect = value["e"] | light.effect;
    light.speed = value["s"] | light.speed;
  }
  state_.dirty = true;
  xSemaphoreGive(stateMutex_);
}

void CodexMicroBle::updateLightingSide(LightingSide& side, JsonObjectConst value) {
  if (value.isNull()) {
    return;
  }
  side.color = value["c"] | side.color;
  side.brightness = value["b"] | side.brightness;
  side.effect = value["e"] | side.effect;
  side.speed = value["s"] | side.speed;
}



