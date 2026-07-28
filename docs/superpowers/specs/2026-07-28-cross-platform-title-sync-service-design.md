# Cross-platform title-sync service design

## Goal

Provide one repository-managed command that installs, inspects, and removes the
Codex Micro USB title synchronizer on both macOS and Windows:

```sh
python tools/title_sync_service.py install
python tools/title_sync_service.py status
python tools/title_sync_service.py uninstall
```

The installer must not require administrator privileges and must not commit or
print a developer's username, repository location, USB serial number, task
title, or other machine-specific value.

## Scope

The change is host-side only. It does not alter the M5StickC firmware, BLE HID
descriptor, vendor RPC protocol, or PlatformIO configuration.

The service supports the title sources already handled by
`tools/sync_titles_usb.py`:

- Recent tasks from the local read-only Codex thread database.
- Explicit Custom Agent Key assignments from the local Codex state file.

Pinned and Priority title mirroring remain out of scope.

## Architecture

`tools/title_sync_service.py` is a standard-library-only service manager with
three subcommands:

- `install` locates the repository, creates an isolated runtime environment,
  installs the pinned `pyserial` dependency, generates an OS-native user
  service definition, and starts it.
- `status` reports whether the generated service definition exists and whether
  the service manager considers it active.
- `uninstall` stops and removes the generated service definition. It preserves
  the isolated environment so reinstalling does not require another download.

The service invokes `tools/sync_titles_usb.py` without a fixed `--port`.
Existing VID/PID and name-based serial discovery therefore handles COM ports on
Windows and `/dev/cu.*` devices on macOS, including a device that is unplugged
and later reconnected.

## Runtime installation

The service manager creates `.title-sync-venv` beneath the repository root and
installs the exact supported `pyserial` version into it. The directory is
ignored by Git.

Installation validates:

- The operating system is macOS or Windows.
- The repository contains the synchronizer.
- A usable Python interpreter can create a virtual environment.
- The generated service configuration passes platform validation.

Repeated installation replaces the existing definition for this application
and restarts the service. It does not touch unrelated services.

## macOS backend

The macOS backend generates a property list locally under the current user's
`~/Library/LaunchAgents` directory. It uses:

- A stable reverse-DNS label.
- `RunAtLoad` and `KeepAlive`.
- The isolated Python interpreter and repository synchronizer resolved at
  install time.
- User-local log files under `~/Library/Logs`.

The backend uses the current GUI user domain with `launchctl bootstrap`,
`bootout`, and `print`. No `sudo` operation is required.

## Windows backend

The Windows backend registers a current-user Task Scheduler task that runs at
logon with limited privileges. The task launches the isolated Python
interpreter and repository synchronizer with no console window.

The backend uses `schtasks.exe` to create, query, run, and delete only the
stable Codex Micro task name. No administrator privileges, saved password, or
machine-wide service is required.

## Privacy

Repository content contains only relative paths, stable service identifiers,
and fictitious test data. Absolute interpreter and repository paths are
resolved during installation and written only to the user's local service
definition.

The synchronizer reads Codex state and writes titles only to the locally
attached StickC serial device. It performs no network transmission.

Daemon logs redact task titles by default. They record connection state,
assigned-slot count, acknowledgements, and errors. An explicit foreground
debug option may display titles when the user requests it.

Tests generate configurations in temporary directories with fictitious
usernames and assert that repository fixtures and normal log output contain no
real home directory, USB serial number, or task title.

## Failure handling

The synchronizer already waits and retries when the Codex state file or serial
device is unavailable. The OS service restarts the process if it exits
unexpectedly.

The installer fails with an actionable error when dependency installation,
service registration, or platform validation fails. It does not report success
unless the service can be queried after registration.

`uninstall` is idempotent: an already absent service is reported as absent
rather than treated as an error.

## Testing

Unit tests cover:

- Recent and Custom title selection.
- Title length normalization.
- Redacted daemon logging.
- macOS plist generation using fictitious paths.
- Windows scheduled-task command generation using fictitious paths.
- Install, status, and uninstall command dispatch with subprocess boundaries
  replaced by controlled test doubles.
- Repeated installation and idempotent uninstall.
- Privacy scans that reject developer-specific paths and identifiers.

Manual validation covers:

- macOS service registration, running state, reconnect behavior, and a
  `TITLE_SYNC_OK` acknowledgement from a connected StickC.
- Windows commands are unit-tested in this macOS change; final real-device
  Windows validation remains documented as required because this environment
  cannot execute Task Scheduler.

## Distribution

The README documents the unified commands, supported title sources, local-only
data flow, generated file locations, logs, and removal procedure. Existing
manual invocation remains supported for troubleshooting.
