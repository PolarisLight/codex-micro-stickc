#pragma once

#include <stdint.h>

struct BleAdvertisingPolicy {
  uint16_t intervalUnits;
  uint16_t intervalMs;
};

constexpr BleAdvertisingPolicy advertisingPolicy(uint8_t connectionCount) {
  return connectionCount == 0 ? BleAdvertisingPolicy{0, 0}
                              : BleAdvertisingPolicy{1600, 1000};
}
