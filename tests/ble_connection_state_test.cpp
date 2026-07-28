#include "BleConnectionState.h"

static_assert(bleConnectionAdded(0) == 1, "first connection is tracked");
static_assert(bleConnectionAdded(1) == 2, "second connection is tracked");
static_assert(bleConnectionRemoved(2) == 1,
              "probe disconnect preserves HID connection");
static_assert(bleConnectionRemoved(0) == 0, "count never underflows");
static_assert(bleHasConnections(1), "one connection remains connected");
static_assert(!bleHasConnections(0), "zero connections is disconnected");

int main() { return 0; }
