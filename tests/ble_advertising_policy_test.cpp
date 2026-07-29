#include "BleAdvertisingPolicy.h"

static_assert(advertisingPolicy(0).intervalUnits == 0,
              "disconnected devices use NimBLE fast defaults");
static_assert(advertisingPolicy(0).intervalMs == 0,
              "default advertising interval is library controlled");
static_assert(advertisingPolicy(1).intervalUnits == 1600,
              "connected devices advertise every second");
static_assert(advertisingPolicy(1).intervalMs == 1000,
              "connected interval is documented in milliseconds");
static_assert(advertisingPolicy(2).intervalUnits ==
                  advertisingPolicy(1).intervalUnits,
              "additional clients remain in low-power advertising");

int main() { return 0; }
