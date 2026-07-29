#pragma once

#include <stdint.h>

struct OrientationDecision {
  bool valid;
  uint8_t rotation;
};

constexpr float orientationAbs(float value) {
  return value < 0.0f ? -value : value;
}

constexpr OrientationDecision gravityOrientation(float ax, float ay) {
  return orientationAbs(ax) > orientationAbs(ay) + 0.15f &&
                 orientationAbs(ax) > 0.55f
             ? OrientationDecision{
                   true, static_cast<uint8_t>(ax > 0.0f ? 1 : 3)}
         : orientationAbs(ay) > orientationAbs(ax) + 0.15f &&
                   orientationAbs(ay) > 0.55f
             ? OrientationDecision{true, 0U}
             : OrientationDecision{false, 0U};
}
