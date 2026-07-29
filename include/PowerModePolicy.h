#pragma once

#include <stdint.h>

struct PowerModePolicy {
  uint16_t cpuMhz;
  uint16_t loopDelayMs;
  bool sleepImu;
};

constexpr PowerModePolicy powerModePolicy(bool screenOff) {
  return screenOff ? PowerModePolicy{80, 50, true}
                   : PowerModePolicy{240, 8, false};
}
