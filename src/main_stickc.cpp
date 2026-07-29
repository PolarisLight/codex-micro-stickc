// SPDX-License-Identifier: MIT
// M5StickC Codex Micro: glanceable task UI with dynamic local title sync.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <esp32-hal-cpu.h>

#include "BatteryEstimator.h"
#include "CodexMicroBle.h"
#include "PowerModePolicy.h"
#include "TitlePersistencePolicy.h"
#include "TitleUiMode.h"

RTC_DATA_ATTR uint32_t recoveryBootMagic = 0;

namespace {

enum class Page : uint8_t { Tasks, Commands };
enum class ScreenPower : uint8_t { Active, Dim, Off };
struct Command { const char* key; const char* label; };

constexpr uint16_t kBackground = 0x0821;
constexpr uint16_t kPanel = 0x10A2;
constexpr uint16_t kText = 0xFFFF;
constexpr uint16_t kMuted = 0x94B2;
constexpr uint16_t kAccent = 0x35BF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kOrange = 0xFD20;
constexpr uint16_t kRed = 0xF800;
constexpr uint32_t kLongPressMs = 650;
constexpr uint32_t kFeedbackMs = 550;
constexpr uint32_t kBatteryDimMs = 20000;
constexpr uint32_t kBatteryOffMs = 60000;
constexpr uint32_t kPoweredDimMs = 120000;
constexpr uint32_t kRecoveryBootMagic = 0xC0DE520U;

const char* kAgentKeys[] = {"AG00", "AG01", "AG02", "AG03", "AG04", "AG05"};
const Command kCommands[] = {
    {"ACT07", "APPROVE"}, {"ACT08", "DECLINE"}, {"ACT10", "MIC / PTT"},
    {"ACT12", "SEND"}, {"ACT06", "FAST"}, {"ACT09", "FORK"},
};

CodexMicroBle codex;
CodexMicroState state;
Preferences preferences;
String agentLabels[6];
String persistedLabels[6];
bool labelAssigned[6] = {};
bool titleSyncActive = false;
Page page = Page::Tasks;
uint8_t selectedAgent = 0;
uint8_t selectedCommand = 0;
BatteryEstimator battery;
ScreenPower screenPower = ScreenPower::Active;
uint32_t lastActivityMs = 0;
bool suppressAUntilRelease = false;
bool suppressBUntilRelease = false;
bool vbusInitialized = false;
bool lastVbusPresent = false;
uint32_t lastVbusPollMs = 0;
uint32_t aPressedAt = 0;
bool aLongHandled = false;
const char* activeKey = nullptr;
int8_t activeAgent = -1;
String feedbackText;
uint16_t feedbackColor = kAccent;
uint32_t feedbackUntil = 0;
bool feedbackVisible = false;
uint8_t displayRotation = 0;
uint8_t pendingRotation = 0;
uint32_t orientationPendingSince = 0;
uint32_t lastImuMs = 0;
uint32_t orientationLockUntil = 0;
float gyroTurnDegrees = 0.0f;
int batteryLevel = -1;
int batteryVoltageMv = 0;
bool batteryCharging = false;
bool batteryExternalPower = false;
int batteryCurrentMa = 0;
bool imuSleeping = false;

uint16_t rgb888To565(uint32_t color, float brightness = 1.0f) {
  const uint8_t red = static_cast<uint8_t>(((color >> 16) & 0xFF) * brightness);
  const uint8_t green = static_cast<uint8_t>(((color >> 8) & 0xFF) * brightness);
  const uint8_t blue = static_cast<uint8_t>((color & 0xFF) * brightness);
  return M5.Display.color565(red, green, blue);
}

size_t utf8CharLength(uint8_t lead) {
  if ((lead & 0x80) == 0) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

void drawCentered(const String& text, int x, int y, uint16_t color = kText) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(color, kBackground);
  M5.Display.drawString(text, x, y);
}

int wrapTitle(const String& title, String lines[], int maxWidth, int maxLines) {
  M5.Display.setFont(&fonts::efontCN_24);
  int count = 0;
  String line;
  for (size_t pos = 0; pos < title.length() && count < maxLines;) {
    const size_t bytes =
        min(utf8CharLength(static_cast<uint8_t>(title[pos])), title.length() - pos);
    const String token = title.substring(pos, pos + bytes);
    const String candidate = line + token;
    if (!line.isEmpty() && M5.Display.textWidth(candidate) > maxWidth) {
      lines[count++] = line;
      line = token;
    } else {
      line = candidate;
    }
    pos += bytes;
  }
  if (!line.isEmpty() && count < maxLines) lines[count++] = line;
  return count;
}

void drawTitleInRegion(const String& title, int centerX, int top, int regionHeight,
                       int maxWidth, int maxLines) {
  constexpr int kLineHeight = 27;
  String lines[4];
  const int count = wrapTitle(title, lines, maxWidth, maxLines);
  const int firstY = top + max(0, (regionHeight - count * kLineHeight) / 2);
  M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(kText, kBackground);
  for (int i = 0; i < count; ++i) {
    M5.Display.drawString(lines[i], centerX, firstY + i * kLineHeight);
  }
}

void drawTaskIdentity(int centerX, int top, int regionHeight, int maxWidth,
                      int maxLines) {
  if (titleSyncActive) {
    drawTitleInRegion(agentLabels[selectedAgent], centerX, top, regionHeight,
                      maxWidth, maxLines);
    return;
  }
  M5.Display.setFont(maxWidth < 100 ? &fonts::Font2 : &fonts::Font4);
  drawCentered(String("AGENT ") + (selectedAgent + 1), centerX,
               top + regionHeight / 2, kText);
}

const char* statusLabel(const ThreadLight& light, bool assigned) {
  if (!assigned) return "UNASSIGNED";
  const uint8_t r = (light.color >> 16) & 0xFF;
  const uint8_t g = (light.color >> 8) & 0xFF;
  const uint8_t b = light.color & 0xFF;
  if (r > 180 && g < 120) return "ERROR";
  if (r > 180 && g > 80 && g < 210) return "ACTION";
  if (b > 150 && b > r && b >= g) return "RUNNING";
  if (g > 170 && r < 100) return light.effect == "off" ? "READY" : "COMPLETE";
  if (light.effect != "off") return "ACTIVE";
  return "READY";
}

uint16_t statusColorFor(const ThreadLight& light, const char* status) {
  const float brightness = light.brightness <= 0.01f ? 0.48f : max(0.48f, light.brightness);
  uint16_t color = light.color == 0 ? kMuted : rgb888To565(light.color, brightness);
  if (strcmp(status, "ACTION") == 0) color = kOrange;
  if (strcmp(status, "ERROR") == 0) color = kRed;
  return color;
}

void drawHeader(int width) {
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(kMuted, kBackground);
  const String leftHeader = !state.connected && state.disconnectReason != 0
                                ? String("BLE R") + state.disconnectReason
                                : (page == Page::Tasks
                                       ? String(selectedAgent + 1) + "/6"
                                       : "ACTION");
  M5.Display.drawString(leftHeader, 5, 4);
  M5.Display.setTextDatum(top_right);
  String headerRight =
      page == Page::Commands ? String(selectedCommand + 1) + "/6" : "";
  if (batteryLevel >= 0) {
    if (!headerRight.isEmpty()) headerRight += " ";
    headerRight += String(batteryLevel) + "%";
  }
  M5.Display.drawString(headerRight, width - 17, 4);
  const uint16_t linkColor = state.ready ? kGreen : (state.connected ? kOrange : kRed);
  M5.Display.fillCircle(width - 7, 9, 4, linkColor);
  M5.Display.drawFastHLine(4, 20, width - 8, kPanel);
}

void drawStatusCard(int x, int y, int width, int height, const char* status,
                    uint16_t statusColor, bool compact) {
  M5.Display.fillRoundRect(x, y, width, height, 8, kPanel);
  M5.Display.drawRoundRect(x, y, width, height, 8, statusColor);
  M5.Display.drawRoundRect(x + 1, y + 1, width - 2, height - 2, 7, statusColor);
  M5.Display.setFont(compact || strlen(status) > 8 ? &fonts::Font2 : &fonts::Font4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(statusColor, kPanel);
  M5.Display.drawString(status, x + width / 2, y + height / 2);
}

void drawPortraitFooter(int width) {
  constexpr int top = 193;
  const int buttonWidth = (width - 15) / 2;
  const char* rightAction = page == Page::Tasks ? "OPEN" : "RUN";
  M5.Display.fillRoundRect(5, top, buttonWidth, 42, 8, kPanel);
  M5.Display.fillRoundRect(10 + buttonWidth, top, buttonWidth, 42, 8, kPanel);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(kText, kPanel);
  M5.Display.drawString("A", 5 + buttonWidth / 2, top + 12);
  M5.Display.drawString("B", 10 + buttonWidth + buttonWidth / 2, top + 12);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(kMuted, kPanel);
  M5.Display.drawString("NEXT/HOLD", 5 + buttonWidth / 2, top + 31);
  M5.Display.drawString(rightAction, 10 + buttonWidth + buttonWidth / 2, top + 31);
}

void drawLandscapeFooter(int width, int height) {
  const int top = height - 31;
  const int buttonWidth = (width - 15) / 2;
  const char* rightAction = page == Page::Tasks ? "B  OPEN" : "B  RUN";
  M5.Display.fillRoundRect(5, top, buttonWidth, 27, 7, kPanel);
  M5.Display.fillRoundRect(10 + buttonWidth, top, buttonWidth, 27, 7, kPanel);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(kText, kPanel);
  M5.Display.drawString("A  NEXT/HOLD", 5 + buttonWidth / 2, top + 14);
  M5.Display.drawString(rightAction, 10 + buttonWidth + buttonWidth / 2, top + 14);
}

void drawPortraitTask(int width) {
  const ThreadLight& light = state.threads[selectedAgent];
  const char* status = statusLabel(
      light, taskStatusAssigned(titleSyncActive, labelAssigned[selectedAgent]));
  drawTaskIdentity(width / 2, 25, 108, width - 10, 4);
  drawStatusCard(5, 137, width - 10, 50, status,
                 statusColorFor(light, status), false);
}

void drawLandscapeTask(int width, int height) {
  const ThreadLight& light = state.threads[selectedAgent];
  const char* status = statusLabel(
      light, taskStatusAssigned(titleSyncActive, labelAssigned[selectedAgent]));
  constexpr int splitX = 151;
  drawTaskIdentity(splitX / 2, 22, 78, splitX - 10, 3);
  drawStatusCard(splitX + 2, 24, width - splitX - 7, 74, status,
                 statusColorFor(light, status), true);
}

void drawPortraitCommand(int width) {
  const Command& command = kCommands[selectedCommand];
  M5.Display.setFont(&fonts::Font4);
  drawCentered(command.label, width / 2, 79, kText);
  M5.Display.fillRoundRect(8, 126, width - 16, 54, 9,
                           selectedCommand == 0 ? kGreen : kAccent);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(0x0000, selectedCommand == 0 ? kGreen : kAccent);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("PRESS B", width / 2, 153);
}

void drawLandscapeCommand(int width) {
  const Command& command = kCommands[selectedCommand];
  M5.Display.setFont(&fonts::Font4);
  drawCentered(command.label, 77, 61, kText);
  const uint16_t color = selectedCommand == 0 ? kGreen : kAccent;
  M5.Display.fillRoundRect(153, 24, width - 158, 74, 8, color);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(0x0000, color);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("PRESS B", 153 + (width - 158) / 2, 61);
}

void drawScreen();

uint8_t activeBrightness() {
  if (batteryExternalPower) return 110;
  return batteryLevel >= 0 && batteryLevel <= 20 ? 55 : 75;
}

void setScreenPower(ScreenPower mode) {
  if (mode == screenPower) return;
  const bool wasOff = screenPower == ScreenPower::Off;
  screenPower = mode;
  if (mode == ScreenPower::Off) {
    M5.Display.setBrightness(0);
    M5.Display.sleep();
    if (M5.Imu.isEnabled()) {
      imuSleeping = M5.Imu.sleep();
    }
    const PowerModePolicy policy = powerModePolicy(true);
    const bool frequencyChanged = setCpuFrequencyMhz(policy.cpuMhz);
    Serial.printf("SCREEN off cpu_mhz=%u imu_sleep=%d freq_ok=%d\n",
                  getCpuFrequencyMhz(), imuSleeping, frequencyChanged);
    return;
  }
  if (wasOff) {
    const PowerModePolicy policy = powerModePolicy(false);
    const bool frequencyChanged = setCpuFrequencyMhz(policy.cpuMhz);
    if (imuSleeping) {
      M5.Imu.begin(&M5.In_I2C, M5.getBoard());
      imuSleeping = false;
      lastImuMs = 0;
      gyroTurnDegrees = 0.0f;
    }
    Serial.printf("SCREEN wake cpu_mhz=%u freq_ok=%d\n",
                  getCpuFrequencyMhz(), frequencyChanged);
  }
  M5.Display.wakeup();
  M5.Display.setBrightness(mode == ScreenPower::Dim
                               ? static_cast<uint8_t>(batteryExternalPower ? 35 : 16)
                               : activeBrightness());
  Serial.printf("SCREEN %s\n", mode == ScreenPower::Dim ? "dim" : "active");
  drawScreen();
}

void noteActivity() {
  lastActivityMs = millis();
  if (screenPower != ScreenPower::Active) setScreenPower(ScreenPower::Active);
}

void updateScreenPower() {
  const uint32_t idle = millis() - lastActivityMs;
  if (batteryExternalPower) {
    if (idle >= kPoweredDimMs && screenPower == ScreenPower::Active) {
      setScreenPower(ScreenPower::Dim);
    }
    return;
  }
  if (idle >= kBatteryOffMs) setScreenPower(ScreenPower::Off);
  else if (idle >= kBatteryDimMs && screenPower == ScreenPower::Active) {
    setScreenPower(ScreenPower::Dim);
  }
}

uint32_t stateSignature(const CodexMicroState& value) {
  uint32_t hash = value.connected ? 0x811C9DC5U : 0x01000193U;
  hash = (hash ^ (value.ready ? 1U : 0U)) * 16777619U;
  hash = (hash ^ static_cast<uint32_t>(value.disconnectReason)) * 16777619U;
  for (const auto& light : value.threads) {
    hash = (hash ^ light.color) * 16777619U;
    hash = (hash ^ static_cast<uint32_t>(light.brightness * 100.0f)) * 16777619U;
    hash = (hash ^ (light.effect.length() ? static_cast<uint8_t>(light.effect[0]) : 0U)) * 16777619U;
  }
  return hash;
}

void drawScreen() {
  if (screenPower == ScreenPower::Off) return;
  const int width = M5.Display.width();
  const int height = M5.Display.height();
  const bool landscape = width > height;
  M5.Display.startWrite();
  M5.Display.fillScreen(kBackground);
  drawHeader(width);
  if (page == Page::Tasks) {
    if (landscape) drawLandscapeTask(width, height);
    else drawPortraitTask(width);
  } else {
    if (landscape) drawLandscapeCommand(width);
    else drawPortraitCommand(width);
  }

  if (feedbackVisible && static_cast<int32_t>(feedbackUntil - millis()) > 0) {
    if (landscape) {
      M5.Display.fillRoundRect(153, 24, width - 158, 74, 8, kPanel);
      M5.Display.setFont(&fonts::Font2);
      drawCentered(feedbackText, 153 + (width - 158) / 2, 61, feedbackColor);
    } else {
      M5.Display.fillRoundRect(8, 142, width - 16, 40, 7, kPanel);
      M5.Display.setFont(&fonts::Font2);
      drawCentered(feedbackText, width / 2, 162, feedbackColor);
    }
  }

  if (landscape) drawLandscapeFooter(width, height);
  else drawPortraitFooter(width);
  M5.Display.endWrite();
}
void loadLabels() {
  preferences.begin("codexpad", false);
  for (int i = 0; i < 6; ++i) {
    const String key = String("label") + i;
    persistedLabels[i] = preferences.getString(key.c_str(), "");
    agentLabels[i] = persistedLabels[i];
    labelAssigned[i] = !persistedLabels[i].isEmpty();
    if (!labelAssigned[i]) agentLabels[i] = String("Agent ") + (i + 1);
  }
}

void applyLabels(const std::array<String, 6>& labels) {
  titleSyncActive = nextTitleUiActive(titleSyncActive, true);
  for (int i = 0; i < 6; ++i) {
    const String next = labels[i];
    if (titleNeedsPersistence(persistedLabels[i].c_str(), next.c_str())) {
      const String key = String("label") + i;
      preferences.putString(key.c_str(), next);
      persistedLabels[i] = next;
    }
    labelAssigned[i] = !next.isEmpty();
    agentLabels[i] =
        labelAssigned[i] ? next : String("Agent ") + (i + 1);
  }
  drawScreen();
}

void processSerialSync() {
  static String input;
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n') {
      DynamicJsonDocument doc(3072);
      if (!deserializeJson(doc, input) && doc["labels"].is<JsonArray>()) {
        JsonArray labels = doc["labels"].as<JsonArray>();
        std::array<String, 6> nextLabels;
        for (int i = 0; i < 6; ++i) {
          nextLabels[i] = i < static_cast<int>(labels.size())
                              ? labels[i].as<String>()
                              : String();
          nextLabels[i].trim();
        }
        applyLabels(nextLabels);
        Serial.println("TITLE_UI titles");
        Serial.println("TITLE_SYNC_OK");
      }
      input.clear();
    } else if (input.length() < 4095) {
      input += ch;
    }
  }
}

void processBleTitleSync() {
  std::array<String, 6> labels;
  if (!codex.takeTitleLabels(labels)) {
    return;
  }
  applyLabels(labels);
  Serial.println("TITLE_UI titles");
  Serial.println("TITLE_SYNC_BLE_OK");
}

void updateOrientation() {
  const uint32_t now = millis();
  if (!M5.Imu.isEnabled() || now - lastImuMs < 20) return;
  const float dt = lastImuMs == 0 ? 0.0f : (now - lastImuMs) / 1000.0f;
  lastImuMs = now;
  if (!M5.Imu.update()) return;

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
  if (!M5.Imu.getGyro(&gx, &gy, &gz)) return;
  if (static_cast<int32_t>(now - orientationLockUntil) < 0) {
    gyroTurnDegrees = 0.0f;
    return;
  }

  uint8_t desired = displayRotation;
  if (fabsf(ax) > fabsf(ay) + 0.15f && fabsf(ax) > 0.55f) {
    desired = ax > 0.0f ? 1 : 3;
  } else if (fabsf(ay) > fabsf(ax) + 0.15f && fabsf(ay) > 0.55f) {
    desired = 0;
  } else {
    // Flat on a desk: gravity is on Z, so integrate a deliberate Z-axis turn.
    if (fabsf(az) > 0.65f && fabsf(gz) > 12.0f) {
      gyroTurnDegrees += gz * dt;
    } else {
      gyroTurnDegrees *= 0.82f;
    }
    if (fabsf(gyroTurnDegrees) >= 52.0f) {
      desired = (displayRotation + (gyroTurnDegrees > 0.0f ? 1 : 3)) & 3;
      gyroTurnDegrees = 0.0f;
      orientationLockUntil = now + 650;
      displayRotation = desired;
      pendingRotation = desired;
      M5.Display.setRotation(displayRotation);
      Serial.printf("ORIENTATION gyro rotation=%u\n", displayRotation);
      drawScreen();
    }
    return;
  }

  gyroTurnDegrees = 0.0f;
  if (desired != pendingRotation) {
    pendingRotation = desired;
    orientationPendingSince = now;
    return;
  }
  if (desired != displayRotation && now - orientationPendingSince >= 350) {
    displayRotation = desired;
    M5.Display.setRotation(displayRotation);
    orientationLockUntil = now + 650;
    Serial.printf("ORIENTATION gravity rotation=%u\n", displayRotation);
    drawScreen();
  }
}
void showFeedback(const String& text, uint16_t color) {
  noteActivity();
  feedbackText = text;
  feedbackColor = color;
  feedbackUntil = millis() + kFeedbackMs;
  feedbackVisible = true;
  drawScreen();
}

void updateBattery() {
  const int oldLevel = batteryLevel;
  const bool oldCharging = batteryCharging;
  const bool oldExternal = batteryExternalPower;
  if (!battery.update()) return;
  batteryLevel = battery.level();
  batteryVoltageMv = battery.voltageMv();
  batteryCurrentMa = battery.currentMa();
  batteryCharging = battery.charging();
  batteryExternalPower = battery.externalPower();
  if (batteryLevel != oldLevel || batteryCharging != oldCharging) {
    codex.setBattery(static_cast<uint8_t>(batteryLevel), batteryCharging,
                     static_cast<int16_t>(batteryVoltageMv));
  }
  if (screenPower == ScreenPower::Active) M5.Display.setBrightness(activeBrightness());
  else if (screenPower == ScreenPower::Dim) {
    M5.Display.setBrightness(batteryExternalPower ? 35 : 16);
  }
  Serial.printf("POWER soc=%.1f level=%d voltage_mv=%d current_ma=%d charging=%d vbus=%d\n",
                battery.soc(), batteryLevel, batteryVoltageMv, batteryCurrentMa,
                batteryCharging, batteryExternalPower);
  if (batteryLevel != oldLevel || batteryCharging != oldCharging ||
      batteryExternalPower != oldExternal) {
    drawScreen();
  }
}

void updateVbusRecovery() {
  const uint32_t now = millis();
  if (lastVbusPollMs != 0 && now - lastVbusPollMs < 100) return;
  lastVbusPollMs = now;
  const bool vbusPresent = M5.Power.Axp192.isVBUS();
  if (!vbusInitialized) {
    vbusInitialized = true;
    lastVbusPresent = vbusPresent;
    return;
  }
  const bool linkHadFailed = !state.connected || state.disconnectReason != 0;
  if (vbusPresent && !lastVbusPresent && linkHadFailed) {
    showFeedback("RECOVER", kOrange);
    recoveryBootMagic = kRecoveryBootMagic;
    delay(40);
    ESP.restart();
  }
  lastVbusPresent = vbusPresent;
}

void startSelectedAction() {
  if (!state.connected || !state.ready || activeKey != nullptr) {
    if (!state.connected) {
      showFeedback("NO LINK", kRed);
    } else if (!state.ready) {
      showFeedback("SYNC", kOrange);
    }
    return;
  }
  if (page == Page::Tasks) {
    activeKey = kAgentKeys[selectedAgent];
    activeAgent = selectedAgent;
  } else {
    activeKey = kCommands[selectedCommand].key;
    activeAgent = -1;
  }
  codex.sendKey(activeKey, 1, activeAgent);
}

void finishSelectedAction() {
  if (activeKey == nullptr) return;
  codex.sendKey(activeKey, 0, activeAgent);
  activeKey = nullptr;
  activeAgent = -1;
  showFeedback("SENT", kGreen);
}

void nextSelection() {
  if (page == Page::Tasks) selectedAgent = (selectedAgent + 1) % 6;
  else selectedCommand = (selectedCommand + 1) % 6;
  drawScreen();
}

void togglePage() {
  finishSelectedAction();
  page = page == Page::Tasks ? Page::Commands : Page::Tasks;
  drawScreen();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  auto config = M5.config();
  config.clear_display = true;
  config.internal_imu = true;
  M5.begin(config);
  M5.Display.setRotation(0);
  displayRotation = 0;
  pendingRotation = 0;
  screenPower = ScreenPower::Active;
  lastActivityMs = millis();
  M5.Display.setBrightness(110);
  M5.Display.setTextWrap(false);

  const bool recoveryBoot = recoveryBootMagic == kRecoveryBootMagic;
  recoveryBootMagic = 0;
  if (recoveryBoot) {
    M5.Display.fillScreen(kBackground);
    M5.Display.setFont(&fonts::Font2);
    drawCentered("RESET LINK", M5.Display.width() / 2,
                 M5.Display.height() / 2 - 10, kOrange);
    M5.Display.setFont(&fonts::Font0);
    drawCentered("WAIT 3 SEC", M5.Display.width() / 2,
                 M5.Display.height() / 2 + 14, kMuted);
    delay(3000);
  }

  loadLabels();
  battery.begin(preferences);
  batteryLevel = battery.level();
  batteryVoltageMv = battery.voltageMv();
  batteryCurrentMa = battery.currentMa();
  batteryCharging = battery.charging();
  batteryExternalPower = battery.externalPower();
  M5.Display.setBrightness(activeBrightness());

  codex.begin();
  codex.setBattery(static_cast<uint8_t>(batteryLevel), batteryCharging,
                   static_cast<int16_t>(batteryVoltageMv));
  state = codex.snapshot();
  drawScreen();
  Serial.println("TITLE_UI status");
  Serial.printf("POWER_INIT soc=%.1f level=%d voltage_mv=%d current_ma=%d vbus=%d\n",
                battery.soc(), batteryLevel, batteryVoltageMv, batteryCurrentMa,
                batteryExternalPower);
  Serial.println("CODEX_MICRO_READY");
}

void loop() {
  M5.update();
  codex.maintain();
  processSerialSync();
  processBleTitleSync();
  updateVbusRecovery();

  if (M5.BtnPWR.wasClicked()) {
    if (screenPower == ScreenPower::Off) noteActivity();
    else setScreenPower(ScreenPower::Off);
  }

  if (screenPower == ScreenPower::Off &&
      (M5.BtnA.wasPressed() || M5.BtnB.wasPressed())) {
    suppressAUntilRelease = M5.BtnA.isPressed();
    suppressBUntilRelease = M5.BtnB.isPressed();
    noteActivity();
  }

  if (suppressAUntilRelease) {
    if (M5.BtnA.wasReleased()) suppressAUntilRelease = false;
  } else {
    if (M5.BtnA.wasPressed()) {
      noteActivity();
      aPressedAt = millis();
      aLongHandled = false;
    }
    if (M5.BtnA.isPressed() && !aLongHandled &&
        millis() - aPressedAt >= kLongPressMs) {
      aLongHandled = true;
      togglePage();
    }
    if (M5.BtnA.wasReleased() && !aLongHandled) nextSelection();
  }

  if (suppressBUntilRelease) {
    if (M5.BtnB.wasReleased()) suppressBUntilRelease = false;
  } else {
    if (M5.BtnB.wasPressed()) {
      noteActivity();
      startSelectedAction();
    }
    if (M5.BtnB.wasReleased()) finishSelectedAction();
  }

  if (screenPower != ScreenPower::Off) updateOrientation();

  CodexMicroState latest = codex.snapshot();
  if (!latest.connected && activeKey != nullptr) {
    activeKey = nullptr;
    activeAgent = -1;
  }
  const bool meaningfulChange = stateSignature(latest) != stateSignature(state);
  state = latest;
  if (meaningfulChange) {
    noteActivity();
    drawScreen();
  }

  if (feedbackVisible && static_cast<int32_t>(millis() - feedbackUntil) >= 0) {
    feedbackVisible = false;
    drawScreen();
  }
  updateBattery();
  updateScreenPower();
  delay(powerModePolicy(screenPower == ScreenPower::Off).loopDelayMs);
}
