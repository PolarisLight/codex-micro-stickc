# BLE Title Sync Proof-of-Concept

## Goal

Prove on macOS that task titles can reach the M5StickC over BLE without sharing
or modifying the Codex Micro vendor HID report channel.

## Firmware

- Add a custom 128-bit GATT service to the existing `Codex Micro` peripheral.
- Add one encrypted read/write characteristic dedicated to title sync.
- Accept newline-delimited `{"labels":[...]}` JSON split into chunks no larger
  than 48 bytes.
- Buffer at most 4096 bytes. Reject malformed or oversized messages and reset
  the buffer.
- After a valid packet, store six pending labels and expose them to the Arduino
  main loop through a mutex-protected take operation.
- The main loop applies and persists labels using the same behavior as USB
  serial sync, activates title UI, redraws, and makes `TITLE_SYNC_OK` available
  for a characteristic read.
- Keep HID descriptors, vendor reports, pacing, and RPC behavior unchanged.

## macOS Probe

- Add `tools/sync_titles_ble.py` using pinned `bleak`.
- Discover the peripheral by the custom service UUID rather than a hardware
  address.
- Connect, serialize the existing local label source, write 48-byte chunks with
  responses, then read and require `TITLE_SYNC_OK`.
- Support `--once`; do not replace the installed USB background service during
  this proof-of-concept.
- Redact titles and device identifiers from default output.

## Validation

- Unit-test payload chunking and title redaction.
- Compile the firmware.
- Stop USB title sync, flash, and re-pair only if macOS has cached the old GATT
  table.
- Send one BLE update and confirm acknowledgement plus title UI activation.
- Leave ChatGPT connected long enough to confirm HID status updates and button
  actions still work.
