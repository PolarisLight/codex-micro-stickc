#pragma once

#include <stdint.h>

constexpr uint8_t bleConnectionAdded(uint8_t count) {
  return count == UINT8_MAX ? count : count + 1;
}

constexpr uint8_t bleConnectionRemoved(uint8_t count) {
  return count == 0 ? 0 : count - 1;
}

constexpr bool bleHasConnections(uint8_t count) { return count != 0; }
