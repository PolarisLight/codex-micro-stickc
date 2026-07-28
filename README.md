# Codex Micro for M5StickC

Unofficial firmware that turns an original **M5StickC** into a compact wireless
controller and status display for Codex Micro features in the ChatGPT desktop
app.

The device presents itself as a BLE vendor HID controller. It can display six
assigned Codex tasks, show their current state, open a selected task, and invoke
remappable Codex Micro actions. The firmware was developed and tested on an
original M5StickC (ESP32-PICO-D4, 80 × 160 display).

> [!WARNING]
> This is an independent compatibility project. It is not affiliated with or
> supported by OpenAI, Work Louder, or M5Stack. It uses an undocumented vendor
> protocol which may change without notice.

## Highlights

- Six Codex task slots with live status colors
- Dynamically synchronized task titles over USB serial
- Portrait and landscape layouts selected by the built-in IMU
- Two-button interface designed for the original M5StickC
- Remappable command page for Approve, Decline, Mic/PTT, Send, Fast, and Fork
- Screen power management that leaves BLE running
- Hybrid battery state-of-charge estimator:
  - integrates signed charge/discharge current
  - uses smoothed, load-compensated voltage only as a slow correction
  - prevents percentage reversals within a charge or discharge phase
  - persists the estimate across restarts
- Automatic BLE advertising and USB-power recovery behavior

## Controls

| Input | Action |
| --- | --- |
| Front A, short press | Select next task or command |
| Front A, hold | Switch between Tasks and Commands |
| Top-right B, press/release | Open the selected task or run the selected command |
| Side power button, short press | Turn the display off or wake it |
| Rotate the device | Switch between portrait and landscape UI |

When the display is asleep, the first A/B press only wakes it. Press again to
perform the action. This avoids accidental task changes while waking the screen.

## Power behavior

The firmware deliberately does **not** put the ESP32 or BLE controller into
light/deep sleep, because doing so can destabilize the active Codex Micro link.
It saves power by managing the display and IMU polling:

| Power source | Behavior |
| --- | --- |
| Battery | Dim after 20 seconds; display off after 60 seconds |
| USB/VBUS | Dim after 120 seconds; display stays on |

The original M5StickC has a very small battery. Runtime will still be limited
while maintaining an active BLE connection.

## Battery estimation

M5Unified's instantaneous voltage-derived percentage changes visibly when the
display, radio, USB power, or CPU load changes. This firmware instead estimates
state of charge using current integration against the original StickC's nominal
95 mAh cell capacity. A smoothed voltage curve is used only for gradual drift
correction and empty/full endpoint calibration.

The first boot starts from a voltage estimate. Accuracy improves after a full
charge and normal discharge cycle. This is still an estimate, not a laboratory
fuel gauge.

## Requirements

- Original M5StickC
- Data-capable USB-C cable
- Windows or macOS with ChatGPT Desktop and Codex Micro support
- [PlatformIO](https://platformio.org/)
- Python 3.10+ with `pyserial` for PlatformIO and optional title synchronization

## Build and flash

```sh
pio run
pio run --target upload
pio device monitor
```

The serial monitor runs at `115200` baud. A successful boot prints:

```text
CODEX_MICRO_READY
```

If automatic upload reset is unreliable, flash the application image at address
`0x10000` using esptool at `115200` baud.

## Pairing

1. Flash and restart the M5StickC.
2. Pair the Bluetooth device named **Codex Micro** with the computer.
3. Open ChatGPT Desktop.
4. Open **Settings → Codex Micro**.
5. Assign tasks to Agent Keys and configure command actions.

If the host has cached an older HID descriptor, forget the device in Bluetooth
settings, restart the M5StickC, and pair again.

## Task-title synchronization

The Codex Micro protocol supplies slot colors and effects but not task titles.
This repository includes a local USB serial helper that sends the six titles to
the StickC. It follows Codex's default **Recent** task source from the local
read-only thread database and also supports explicit **Custom** assignments.

Install the current-user background service with the same command on macOS and
Windows (administrator access is not required):

```sh
python tools/title_sync_service.py install
python tools/title_sync_service.py status
python tools/title_sync_service.py uninstall
```

The installer copies the helper into the current user's application-data
directory, creates an isolated `.title-sync-venv` there, installs the pinned
serial dependency, and registers the service with launchd on macOS or Task
Scheduler on Windows. It automatically discovers the StickC serial port, so the
same setup continues to work if the port name changes.

For foreground troubleshooting:

```sh
python -m pip install pyserial
python tools/sync_titles_usb.py
python tools/sync_titles_usb.py --show-titles
```

Use `--port COM3` or `--port /dev/cu.usbserial-*` only when automatic discovery
cannot choose between multiple serial devices.
The helper only reads the local Codex assignment state and writes titles over
USB serial. It does not send titles through BLE or to a network service.
Background logs redact task names by default; `--show-titles` is an explicit
foreground debugging option.

Pinned and Priority task sources are not yet mirrored by the helper. Select
Recent or Custom in Codex Micro settings when title synchronization is needed.

Do not run a second host writer against the active vendor BLE HID channel.
Testing showed that concurrent HID writers can cause link-layer timeouts and
make reconnection unreliable.

## Status display

The UI interprets host-supplied task colors approximately as:

| Display | Meaning |
| --- | --- |
| Blue | Running |
| Orange | Action or approval required |
| Green | Ready or completed |
| Red | Error |
| Unassigned | No task is assigned to the slot |

Exact colors and effects are controlled by the host and may change with
ChatGPT Desktop updates.

## Known limitations

- Tested on the original M5StickC; M5StickC Plus variants are not yet validated.
- The built-in microphone is not streamed to the computer.
- The Mic/PTT action currently activates the host computer's microphone.
- Task-title sync requires USB because the vendor HID channel must remain
  single-owner.
- The vendor protocol is undocumented and may break after a desktop app update.
- BLE reliability is sensitive to the original StickC's small battery and power
  path; keep a known-good firmware image available for recovery.

## Project layout

```text
include/BatteryEstimator.h   Hybrid state-of-charge estimator
include/CodexMicroBle.h      BLE transport and shared state
src/CodexMicroBle.cpp        HID descriptor, RPC framing, and host protocol
src/main_stickc.cpp          UI, controls, IMU, title sync, and power behavior
tools/sync_titles_usb.py     Local Codex assignment-title synchronizer
tools/title_sync_service.py  One-command macOS/Windows service manager
platformio.ini               Reproducible PlatformIO build
```

## Credits

This port is derived from
[`imliubo/codex-micro-4-core2`](https://github.com/imliubo/codex-micro-4-core2),
which provided the original BLE HID and Codex Micro protocol implementation.
The original copyright and MIT license are preserved.

## License

MIT. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).
