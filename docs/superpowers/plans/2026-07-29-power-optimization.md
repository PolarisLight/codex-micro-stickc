# StickC Power Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce battery drain from BLE title synchronization and make screen-off mode lower power without disconnecting Codex HID.

**Architecture:** Pure C++ policy headers define advertising and screen-power decisions so they can be tested natively. Hardware-facing code applies those decisions to NimBLE, CPU frequency, display, and IMU. The Python title service retains immediate assignment updates while changing its unchanged-title recovery interval to five minutes.

**Tech Stack:** Arduino C++, M5Unified, NimBLE-Arduino, Python 3 `unittest`, PlatformIO.

---

### Task 1: Slow advertising after connection

**Files:**
- Create: `include/BleAdvertisingPolicy.h`
- Create: `tests/ble_advertising_policy_test.cpp`
- Modify: `src/CodexMicroBle.cpp`

- [ ] **Step 1: Write the failing native test**

```cpp
#include <cassert>
#include "BleAdvertisingPolicy.h"

int main() {
  const auto disconnected = advertisingPolicy(0);
  assert(disconnected.intervalUnits == 0);
  const auto connected = advertisingPolicy(1);
  assert(connected.intervalUnits == 1600);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```sh
c++ -std=c++11 -Iinclude tests/ble_advertising_policy_test.cpp -o /tmp/ble_advertising_policy_test
```

Expected: compilation fails because `BleAdvertisingPolicy.h` does not exist.

- [ ] **Step 3: Implement and apply the policy**

Create a header returning `0` for NimBLE defaults when there are no connections and `1600` units for a one-second interval when at least one connection exists. In `CodexMicroBle`, store the advertising object and apply the policy after every connection-count change before restarting advertising.

- [ ] **Step 4: Run the native test**

Run:

```sh
c++ -std=c++11 -Iinclude tests/ble_advertising_policy_test.cpp -o /tmp/ble_advertising_policy_test
/tmp/ble_advertising_policy_test
```

Expected: exit status 0.

### Task 2: Reduce unchanged-title refresh traffic

**Files:**
- Modify: `tests/test_sync_titles_auto.py`
- Modify: `tools/sync_titles_auto.py`

- [ ] **Step 1: Change the failing scheduling test**

```python
def test_unchanged_assignments_refresh_after_device_reboot_window(self):
    self.assertFalse(should_sync(["A"], ["A"], 299.9, 0.0))
    self.assertTrue(should_sync(["A"], ["A"], 300.0, 0.0))
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.title-sync-venv/bin/python -m unittest tests.test_sync_titles_auto -v`

Expected: FAIL because the current interval is 30 seconds.

- [ ] **Step 3: Change the refresh constant**

Set `REFRESH_SECONDS = 300.0` without changing immediate synchronization when labels differ.

- [ ] **Step 4: Run the scheduling tests**

Run: `.title-sync-venv/bin/python -m unittest tests.test_sync_titles_auto -v`

Expected: PASS.

### Task 3: Avoid repeated NVS title writes

**Files:**
- Create: `include/TitlePersistencePolicy.h`
- Create: `tests/title_persistence_policy_test.cpp`
- Modify: `src/main_stickc.cpp`

- [ ] **Step 1: Write the failing native test**

```cpp
#include <cassert>
#include "TitlePersistencePolicy.h"

int main() {
  assert(!titleNeedsPersistence("same", "same"));
  assert(titleNeedsPersistence("old", "new"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```sh
c++ -std=c++11 -Iinclude tests/title_persistence_policy_test.cpp -o /tmp/title_persistence_policy_test
```

Expected: compilation fails because the policy header does not exist.

- [ ] **Step 3: Implement conditional persistence**

Compare the incoming title with the current persisted label before updating runtime state. Call `preferences.putString` only when the values differ. Always activate title UI and update runtime labels.

- [ ] **Step 4: Run the native test**

Run the compiled test and expect exit status 0.

### Task 4: Add stable screen-off power mode

**Files:**
- Create: `include/PowerModePolicy.h`
- Create: `tests/power_mode_policy_test.cpp`
- Modify: `src/main_stickc.cpp`

- [ ] **Step 1: Write the failing native test**

```cpp
#include <cassert>
#include "PowerModePolicy.h"

int main() {
  assert(powerModePolicy(false).cpuMhz == 240);
  assert(powerModePolicy(false).loopDelayMs == 8);
  assert(powerModePolicy(true).cpuMhz == 80);
  assert(powerModePolicy(true).loopDelayMs == 50);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```sh
c++ -std=c++11 -Iinclude tests/power_mode_policy_test.cpp -o /tmp/power_mode_policy_test
```

Expected: compilation fails because the policy header does not exist.

- [ ] **Step 3: Apply power transitions**

Before entering screen Off, call `M5.Imu.sleep()`, set the CPU to 80 MHz, and use a 50 ms loop delay. When leaving Off, restore 240 MHz and call `M5.Imu.begin(&M5.In_I2C, M5.getBoard())` before orientation polling resumes.

- [ ] **Step 4: Run the native test**

Run the compiled test and expect exit status 0.

### Task 5: Document, verify, install, and measure

**Files:**
- Modify: `README.md`
- Modify: `README_EN.md`

- [ ] **Step 1: Document the new behavior**

Describe the one-second connected advertising interval, five-minute recovery refresh, and stable screen-off power mode in both README files.

- [ ] **Step 2: Run full verification**

Run:

```sh
.title-sync-venv/bin/python -m unittest discover -v
c++ -std=c++11 -Iinclude tests/title_ui_mode_test.cpp -o /tmp/title_ui_mode_test
/tmp/title_ui_mode_test
c++ -std=c++11 -Iinclude tests/ble_connection_state_test.cpp -o /tmp/ble_connection_state_test
/tmp/ble_connection_state_test
c++ -std=c++11 -Iinclude tests/ble_advertising_policy_test.cpp -o /tmp/ble_advertising_policy_test
/tmp/ble_advertising_policy_test
c++ -std=c++11 -Iinclude tests/title_persistence_policy_test.cpp -o /tmp/title_persistence_policy_test
/tmp/title_persistence_policy_test
c++ -std=c++11 -Iinclude tests/power_mode_policy_test.cpp -o /tmp/power_mode_policy_test
/tmp/power_mode_policy_test
PLATFORMIO_CORE_DIR=.platformio-core .venv312/bin/pio run
git diff --check
```

Expected: all commands exit with status 0.

- [ ] **Step 3: Update the installed service**

Run `python tools/title_sync_service.py install`, then verify status reports running and the installed helper contains `REFRESH_SECONDS = 300.0`.

- [ ] **Step 4: Flash and inspect the device**

Upload the firmware, confirm `CODEX_MICRO_READY`, allow the screen to enter Off mode, then confirm the device remains connected and wakes on the first A or B press.

- [ ] **Step 5: Compare battery current**

With USB removed and the screen Off, sample the AXP192 current before and after the new firmware under the same HID-connected conditions. Report measured current and estimated runtime from the 95 mAh cell.
