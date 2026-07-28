# Firmware Title UI Fallback

## Goal

Keep the M5StickC useful without the optional USB title-sync service. Each boot
starts in a status-only UI based on the reference firmware. The title-oriented
UI becomes active only after the firmware receives a valid title-sync packet in
that boot session.

## Behavior

- Add a volatile `titleSyncActive` flag initialized to `false`.
- Do not use labels persisted from an earlier boot to select the initial UI.
- While the flag is false, the task page shows the six numbered agent slots and
  their host-supplied colors/effects. It does not render task titles or the
  `UNASSIGNED` text.
- A valid newline-delimited JSON object containing a `labels` array sets the
  flag to `true`, updates and persists all six labels, acknowledges the packet,
  and redraws the screen.
- Once activated, title mode remains active until reboot, even if USB later
  disconnects. Empty slots retain the existing title-mode behavior.
- Commands, controls, BLE transport, orientation, battery, and power management
  remain unchanged.

## Validation

- A pure rendering-mode helper is tested for the boot and first-sync
  transitions.
- The firmware is compiled with PlatformIO.
- Host-service tests continue to pass.
