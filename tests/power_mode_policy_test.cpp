#include "PowerModePolicy.h"

static_assert(powerModePolicy(false).cpuMhz == 240,
              "active and dim modes keep full CPU speed");
static_assert(powerModePolicy(false).loopDelayMs == 8,
              "interactive modes keep the responsive loop");
static_assert(powerModePolicy(true).cpuMhz == 80,
              "screen-off mode lowers CPU speed");
static_assert(powerModePolicy(true).loopDelayMs == 50,
              "screen-off mode lowers the polling rate");
static_assert(!powerModePolicy(false).sleepImu,
              "interactive modes keep orientation enabled");
static_assert(powerModePolicy(true).sleepImu,
              "screen-off mode sleeps the IMU");

int main() { return 0; }
