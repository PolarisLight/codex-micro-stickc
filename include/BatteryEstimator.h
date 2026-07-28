#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>

class BatteryEstimator {
 public:
  void begin(Preferences& preferences) {
    preferences_ = &preferences;
    voltageMv_ = M5.Power.getBatteryVoltage();
    currentMa_ = M5.Power.getBatteryCurrent();
    externalPower_ = M5.Power.Axp192.isVBUS();
    charging_ = M5.Power.isCharging() == m5::Power_Class::is_charging;
    const float stored = preferences.getFloat("soc", -1.0f);
    soc_ = stored >= 0.0f && stored <= 100.0f
               ? stored
               : voltageToSoc(compensatedVoltage(voltageMv_, currentMa_));
    level_ = constrain(static_cast<int>(lroundf(soc_)), 0, 100);
    lastSavedSoc_ = soc_;
    lastSampleMs_ = millis();
    lastSaveMs_ = lastSampleMs_;
  }

  bool update() {
    const uint32_t now = millis();
    if (lastSampleMs_ != 0 && now - lastSampleMs_ < kSampleMs) return false;
    const uint32_t dtMs = lastSampleMs_ == 0 ? kSampleMs : min<uint32_t>(now - lastSampleMs_, 30000);
    lastSampleMs_ = now;

    const int rawVoltage = M5.Power.getBatteryVoltage();
    const int rawCurrent = M5.Power.getBatteryCurrent();
    if (rawVoltage >= 2800 && rawVoltage <= 4400) {
      voltageMv_ = voltageMv_ <= 0 ? rawVoltage
                                   : static_cast<int>(lroundf(voltageMv_ * 0.88f + rawVoltage * 0.12f));
    }
    if (abs(rawCurrent) <= 1000) {
      currentMa_ = static_cast<int>(lroundf(currentMa_ * 0.72f + rawCurrent * 0.28f));
    }

    const bool oldExternal = externalPower_;
    const bool oldCharging = charging_;
    const int oldLevel = level_;
    externalPower_ = M5.Power.Axp192.isVBUS();
    charging_ = M5.Power.isCharging() == m5::Power_Class::is_charging;

    float effectiveCurrent = static_cast<float>(currentMa_);
    if (externalPower_ && effectiveCurrent < 0.0f) effectiveCurrent = 0.0f;
    const float deltaMah = effectiveCurrent * (dtMs / 3600000.0f);
    float candidate = soc_ + (deltaMah / kCapacityMah) * 100.0f * (effectiveCurrent > 0.0f ? 0.90f : 1.0f);

    const float voltageSoc = voltageToSoc(compensatedVoltage(voltageMv_, currentMa_));
    const float correctionLimit = max(0.02f, dtMs / 60000.0f * 0.75f);
    const float correction = constrain(voltageSoc - candidate, -correctionLimit, correctionLimit);
    candidate += correction;

    // Source transitions must not make the displayed charge run backwards.
    if (externalPower_) candidate = max(candidate, soc_);
    else candidate = min(candidate, soc_);

    if (externalPower_ && voltageMv_ >= 4140 && abs(currentMa_) <= 10) {
      fullStableMs_ += dtMs;
      if (fullStableMs_ >= 120000) candidate = 100.0f;
    } else {
      fullStableMs_ = 0;
    }
    if (!externalPower_ && voltageMv_ <= 3350) candidate = 0.0f;

    soc_ = constrain(candidate, 0.0f, 100.0f);
    const int estimated = constrain(static_cast<int>(lroundf(soc_)), 0, 100);
    if (externalPower_) level_ = max(level_, estimated);
    else level_ = min(level_, estimated);

    if (preferences_ && now - lastSaveMs_ >= 300000 && fabsf(soc_ - lastSavedSoc_) >= 1.0f) {
      preferences_->putFloat("soc", soc_);
      lastSavedSoc_ = soc_;
      lastSaveMs_ = now;
    }
    return level_ != oldLevel || externalPower_ != oldExternal || charging_ != oldCharging;
  }

  int level() const { return level_; }
  int voltageMv() const { return voltageMv_; }
  int currentMa() const { return currentMa_; }
  bool charging() const { return charging_; }
  bool externalPower() const { return externalPower_; }
  float soc() const { return soc_; }

 private:
  static constexpr float kCapacityMah = 95.0f;
  static constexpr uint32_t kSampleMs = 5000;

  static float compensatedVoltage(int voltageMv, int currentMa) {
    // Approximate the resting voltage of this very small LiPo under load.
    return static_cast<float>(voltageMv) - static_cast<float>(currentMa) * 0.18f;
  }

  static float voltageToSoc(float mv) {
    struct Point { float mv; float soc; };
    static constexpr Point curve[] = {
        {3350, 0}, {3500, 4}, {3600, 9}, {3700, 20}, {3780, 38},
        {3850, 55}, {3920, 70}, {4000, 82}, {4080, 91}, {4160, 97}, {4200, 100},
    };
    if (mv <= curve[0].mv) return curve[0].soc;
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
      if (mv <= curve[i].mv) {
        const float ratio = (mv - curve[i - 1].mv) / (curve[i].mv - curve[i - 1].mv);
        return curve[i - 1].soc + ratio * (curve[i].soc - curve[i - 1].soc);
      }
    }
    return 100.0f;
  }

  Preferences* preferences_ = nullptr;
  float soc_ = 0.0f;
  float lastSavedSoc_ = 0.0f;
  int level_ = -1;
  int voltageMv_ = 0;
  int currentMa_ = 0;
  bool charging_ = false;
  bool externalPower_ = false;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastSaveMs_ = 0;
  uint32_t fullStableMs_ = 0;
};