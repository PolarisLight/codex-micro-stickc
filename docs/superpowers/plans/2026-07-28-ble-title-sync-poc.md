# BLE Title Sync Proof-of-Concept Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Send the six Codex task titles from macOS to the M5StickC through an isolated encrypted BLE GATT service while preserving the existing Codex Micro HID connection.

**Architecture:** `CodexMicroBle` owns a custom title characteristic and buffers chunked newline-delimited JSON. It exposes completed labels to `main_stickc.cpp`, which applies them through the same code path as USB. A separate async Python probe discovers the service, writes 48-byte chunks, and reads an acknowledgement.

**Tech Stack:** Arduino C++17, NimBLE-Arduino 2.5, ArduinoJson 6, Python 3.10+, Bleak 3.0.2, unittest, PlatformIO

---

### Task 1: Preserve the approved UI baseline

**Files:**
- Modify: `src/main_stickc.cpp`
- Create: `include/TitleUiMode.h`
- Create: `tests/title_ui_mode_test.cpp`
- Modify: `README.md`

- [x] **Step 1: Run the already-written transition test**

Run:

```sh
c++ -std=c++11 -Iinclude tests/title_ui_mode_test.cpp -o /tmp/title_ui_mode_test
/tmp/title_ui_mode_test
```

Expected: exit code 0.

- [x] **Step 2: Build the approved default and title UI**

Run:

```sh
PLATFORMIO_CORE_DIR=.platformio-core .venv312/bin/pio run
```

Expected: `SUCCESS`.

- [x] **Step 3: Commit the UI baseline**

Stage only the fallback UI, compact task header, tests, README, and their
existing design/plan documents. Commit as `add firmware title UI fallback`.

### Task 2: Build the macOS BLE probe test-first

**Files:**
- Create: `tools/sync_titles_ble.py`
- Create: `tests/test_sync_titles_ble.py`

- [x] **Step 1: Write failing payload tests**

```python
from tools.sync_titles_ble import build_payload, chunk_payload, sync_summary

def test_payload_is_newline_delimited_and_chunks_are_bounded():
    payload = build_payload(["一" * 40, "two", "", "", "", ""])
    chunks = chunk_payload(payload)
    assert payload.endswith(b"\n")
    assert b"".join(chunks) == payload
    assert all(0 < len(chunk) <= 48 for chunk in chunks)

def test_default_summary_redacts_titles():
    assert sync_summary(["Private", "", "", "", "", ""]) == "1/6 labels"
```

- [x] **Step 2: Run the tests and verify RED**

Run:

```sh
PYTHONPATH=. .venv312/bin/python -m unittest tests.test_sync_titles_ble -v
```

Expected: import failure because `tools.sync_titles_ble` does not exist.

- [x] **Step 3: Implement payload and BLE transport**

Use these constants:

```python
SERVICE_UUID = "5f83a25b-442d-4d56-bf3a-3e2f8b21e101"
TITLE_UUID = "5f83a25b-442d-4d56-bf3a-3e2f8b21e102"
CHUNK_SIZE = 48
ACK = b"TITLE_SYNC_OK"
```

`build_payload(labels)` serializes `{"labels":[...]}` as compact UTF-8 plus a
newline. `chunk_payload(payload)` slices bytes into 48-byte chunks.
`sync_once(labels)` lazily imports `BleakClient` and `BleakScanner`, discovers a
device advertising `SERVICE_UUID`, writes every chunk with `response=True`,
reads `TITLE_UUID`, and raises unless the value is `ACK`. `main()` reads labels
through `tools.sync_titles_usb.read_labels`, supports `--once`, and prints only
`Synced over BLE: N/6 labels`.

- [x] **Step 4: Run tests and verify GREEN**

Run:

```sh
PYTHONPATH=. .venv312/bin/python -m unittest tests.test_sync_titles_ble -v
```

Expected: all BLE probe unit tests pass without Bleak installed.

### Task 3: Add the isolated encrypted GATT endpoint

**Files:**
- Modify: `include/CodexMicroBle.h`
- Modify: `src/CodexMicroBle.cpp`
- Modify: `src/main_stickc.cpp`

- [x] **Step 1: Add the public handoff API**

Add:

```cpp
bool takeTitleLabels(std::array<String, 6>& labels);
```

and private state:

```cpp
class TitleCallbacks;
void onTitleWrite(NimBLECharacteristic* characteristic,
                  const uint8_t* data, size_t length);
NimBLECharacteristic* titleSync_ = nullptr;
String titleRxBuffer_;
std::array<String, 6> pendingTitleLabels_;
bool titleLabelsPending_ = false;
```

Use the existing `stateMutex_` to protect the pending array and flag.

- [x] **Step 2: Create the service before starting the server**

Use the same UUIDs as the Python probe. Create a 512-byte characteristic with:

```cpp
NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE_ENC
```

Set its initial value to `TITLE_SYNC_READY`, attach `TitleCallbacks`, start the
service, add its UUID to advertising, and leave all HID characteristics and
report descriptors unchanged.

- [x] **Step 3: Parse chunked title messages**

Append each characteristic write to `titleRxBuffer_`. Clear it and set
`TITLE_SYNC_ERROR` if it exceeds 4096 bytes. When a newline arrives, parse the
first complete JSON object, require a `labels` array, copy six trimmed strings
into `pendingTitleLabels_` under `stateMutex_`, set `titleLabelsPending_`, clear
the receive buffer, and set the characteristic value to `TITLE_SYNC_OK`.

- [x] **Step 4: Share one label-application path**

Extract `applyLabels(const std::array<String, 6>&)` from the USB parser. Call it
for USB packets and for:

```cpp
std::array<String, 6> labels;
if (codex.takeTitleLabels(labels)) {
  applyLabels(labels);
  Serial.println("TITLE_SYNC_BLE_OK");
}
```

`applyLabels` activates title UI, persists all six strings, acknowledges only
through the caller's transport, and redraws.

- [x] **Step 5: Compile firmware**

Run:

```sh
PLATFORMIO_CORE_DIR=.platformio-core .venv312/bin/pio run
```

Expected: `SUCCESS` with no new compiler warnings.

### Task 4: Perform the macOS proof

**Files:**
- Modify: `README.md`

- [x] **Step 1: Install the pinned probe dependency**

Run:

```sh
.venv312/bin/python -m pip install bleak==3.0.2
```

Expected: Bleak 3.0.2 installed.

- [x] **Step 2: Stop USB sync and flash**

Stop the current-user title service, upload the firmware through the connected
serial port, and confirm `CODEX_MICRO_READY`.

- [x] **Step 3: Refresh pairing only if required**

First scan for `SERVICE_UUID`. If macOS does not expose it because of cached
services, forget `Codex Micro`, restart the StickC, pair again, and reopen
ChatGPT Desktop.

- [x] **Step 4: Send titles over BLE**

Run:

```sh
PYTHONPATH=. .venv312/bin/python tools/sync_titles_ble.py --once
```

Expected: `Synced over BLE: N/6 labels`; the device switches from `AGENT N` to
real titles without USB title writes.

- [x] **Step 5: Check HID coexistence**

Keep USB title sync stopped. Confirm ChatGPT continues to update task colors and
that A/B task actions work after the BLE write.

- [x] **Step 6: Document observed scope**

Document macOS proof status, manual probe command, privacy behavior, re-pairing
note, and that Windows/background BLE remain unvalidated.

### Task 5: Run regression checks and publish

**Files:**
- Verify all modified files

- [ ] **Step 1: Run all automated checks**

Run the native C++ transition test, Python unittest discovery, Python
`py_compile`, PlatformIO build, `git diff --check`, and the repository privacy
scan.

- [ ] **Step 2: Commit and update the existing PR**

Commit the BLE firmware, probe, tests, and documentation. Push
`codex/cross-platform-title-sync-service` and update PR #1 with the macOS BLE
proof result.
