#include "OrientationPolicy.h"

constexpr OrientationDecision portrait =
    gravityOrientation(0.05f, 0.95f);
static_assert(portrait.valid && portrait.rotation == 0,
              "upright portrait remains portrait");

constexpr OrientationDecision landscapeRight =
    gravityOrientation(0.95f, 0.05f);
static_assert(landscapeRight.valid && landscapeRight.rotation == 1,
              "right rotation selects landscape");

constexpr OrientationDecision landscapeLeft =
    gravityOrientation(-0.95f, 0.05f);
static_assert(landscapeLeft.valid && landscapeLeft.rotation == 3,
              "left rotation selects landscape");

constexpr OrientationDecision handheldPortrait =
    gravityOrientation(-0.12f, 0.67f);
static_assert(handheldPortrait.valid && handheldPortrait.rotation == 0,
              "normal handheld portrait tolerates forward viewing angle");

constexpr OrientationDecision faceUp =
    gravityOrientation(-0.09f, 0.14f);
static_assert(!faceUp.valid,
              "face-up placement keeps the current orientation");

int main() { return 0; }
