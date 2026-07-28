# Cross-platform Title Sync Service Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship one privacy-preserving Python command that installs, inspects, and removes the Codex Micro title synchronizer for the current user on macOS and Windows.

**Architecture:** Keep title selection and serial transport in `tools/sync_titles_usb.py`. Add a standard-library-only `tools/title_sync_service.py` that creates a repository-local runtime, then registers the synchronizer through launchd on macOS or Task Scheduler on Windows. Generate all absolute paths only at install time and redact titles from daemon logs by default.

**Tech Stack:** Python 3.9+, `venv`, `plistlib`, `subprocess`, macOS launchd, Windows Task Scheduler, `pyserial==3.5`, `unittest`.

---

## File structure

- `tools/sync_titles_usb.py`: select Recent or Custom titles, normalize them, communicate over USB serial, and emit privacy-safe logs.
- `tools/title_sync_service.py`: cross-platform install/status/uninstall command and OS-specific service definitions.
- `tests/test_sync_titles_usb.py`: title-source, truncation, and redacted-logging tests.
- `tests/test_title_sync_service.py`: pure configuration tests plus mocked service-manager command tests.
- `.gitignore`: ignore the generated `.title-sync-venv` runtime.
- `README.md`: document unified deployment, privacy, logs, status, and removal.

### Task 1: Make synchronizer logs private by default

**Files:**
- Modify: `tools/sync_titles_usb.py`
- Modify: `tests/test_sync_titles_usb.py`

- [ ] **Step 1: Write the failing redaction test**

Add a serial double and a test that exercises the real `send` function:

```python
import contextlib
import io

from tools.sync_titles_usb import send


class AcknowledgingSerial:
    def __init__(self):
        self.payload = b""

    def write(self, payload):
        self.payload += payload

    def flush(self):
        pass

    def readline(self):
        return b"TITLE_SYNC_OK\n"


def test_send_redacts_titles_by_default(self):
    link = AcknowledgingSerial()
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        send(link, ["private task", "", "", "", "", ""])
    self.assertEqual(
        json.loads(link.payload.decode("utf-8"))["labels"][0],
        "private task",
    )
    self.assertNotIn("private task", output.getvalue())
    self.assertIn("1/6 labels", output.getvalue())
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```sh
PYTHONPATH=. python -m unittest -v tests/test_sync_titles_usb.py
```

Expected: FAIL because `send` currently prints every title.

- [ ] **Step 3: Implement redacted logging with explicit foreground opt-in**

Change `send` and its callers:

```python
def send(link, labels, show_titles=False):
    line = json.dumps({"labels": labels}, ensure_ascii=False, separators=(",", ":")) + "\n"
    link.write(line.encode("utf-8"))
    link.flush()
    deadline, acknowledged = time.monotonic() + 1.5, False
    while time.monotonic() < deadline:
        response = link.readline().decode("utf-8", errors="ignore").strip()
        if response == "TITLE_SYNC_OK":
            acknowledged = True
            break
    prefix = "Synced" if acknowledged else "Sent (awaiting device acknowledgement)"
    assigned = sum(bool(label) for label in labels)
    if show_titles:
        detail = " | ".join(label or "(unassigned)" for label in labels)
    else:
        detail = f"{assigned}/6 labels"
    print(f"{prefix}: {detail}", flush=True)


def run(port, path, once, show_titles=False):
    previous, last_send, link = None, 0.0, None
    while True:
        try:
            labels = read_labels(path)
            if link is None or not link.is_open:
                selected = choose_port(port)
                link = open_serial(selected)
                print(f"Connected to {selected}; watching Codex state", flush=True)
                time.sleep(2)
                previous = None
            now = time.monotonic()
            if labels != previous or now - last_send >= 60:
                send(link, labels, show_titles=show_titles)
                previous, last_send = labels, now
                if once:
                    return
            if link.in_waiting:
                link.read(link.in_waiting)
            time.sleep(1)
        except KeyboardInterrupt:
            return
        except Exception as error:
            print(f"Title sync waiting: {error}", file=sys.stderr, flush=True)
            if link:
                try:
                    link.close()
                except Exception:
                    pass
            link = None
            if once:
                raise
            time.sleep(3)


parser.add_argument(
    "--show-titles",
    action="store_true",
    help="include task titles in console output",
)
args = parser.parse_args()
run(args.port, args.state, args.once, args.show_titles)
```

- [ ] **Step 4: Run synchronizer tests and verify GREEN**

Run:

```sh
PYTHONPATH=. python -m unittest -v tests/test_sync_titles_usb.py
```

Expected: all synchronizer tests PASS and normal captured output contains no title.

- [ ] **Step 5: Commit the privacy change**

```sh
git add tools/sync_titles_usb.py tests/test_sync_titles_usb.py
git commit -m "redact title sync service logs"
```

### Task 2: Generate portable macOS and Windows service definitions

**Files:**
- Create: `tools/title_sync_service.py`
- Create: `tests/test_title_sync_service.py`

- [ ] **Step 1: Write failing pure-generation tests**

Create tests using only fictitious temporary paths:

```python
import plistlib
import unittest
from pathlib import Path, PureWindowsPath

from tools.title_sync_service import (
    LABEL,
    TASK_NAME,
    build_macos_plist,
    build_windows_task_command,
)


class ServiceDefinitionTests(unittest.TestCase):
    def test_macos_plist_uses_runtime_values_without_fixed_port(self):
        repo = Path("/tmp/example-user/project")
        python = repo / ".title-sync-venv/bin/python"
        home = Path("/tmp/example-user")
        payload = plistlib.dumps(build_macos_plist(repo, python, home))
        text = payload.decode("utf-8")
        self.assertIn(str(python), text)
        self.assertIn(str(repo / "tools/sync_titles_usb.py"), text)
        self.assertNotIn("--port", text)
        self.assertNotIn("usbserial-", text)
        self.assertIn(LABEL, text)

    def test_windows_task_command_quotes_fictitious_paths(self):
        repo = PureWindowsPath(r"C:\Users\Example User\codex-micro-stickc")
        python = repo / r".title-sync-venv\Scripts\pythonw.exe"
        command = build_windows_task_command(repo, python)
        self.assertIn('"C:\\Users\\Example User', command)
        self.assertIn("sync_titles_usb.py", command)
        self.assertNotIn("--port", command)
        self.assertTrue(TASK_NAME)
```

- [ ] **Step 2: Run the tests and verify RED**

Run:

```sh
PYTHONPATH=. python -m unittest -v tests/test_title_sync_service.py
```

Expected: ERROR because `tools.title_sync_service` does not exist.

- [ ] **Step 3: Implement constants and pure builders**

Create `tools/title_sync_service.py` with:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import plistlib
import subprocess
import sys
import venv
from pathlib import Path

LABEL = "io.github.codex-micro-stickc.title-sync"
TASK_NAME = "Codex Micro Title Sync"
PYSERIAL_REQUIREMENT = "pyserial==3.5"
RUNTIME_DIR = ".title-sync-venv"


def sync_script(repo_root):
    return Path(repo_root) / "tools" / "sync_titles_usb.py"


def runtime_python(repo_root, platform=sys.platform):
    root = Path(repo_root) / RUNTIME_DIR
    if platform == "win32":
        return root / "Scripts" / "pythonw.exe"
    return root / "bin" / "python"


def build_macos_plist(repo_root, python_path, home):
    repo_root, python_path, home = map(Path, (repo_root, python_path, home))
    return {
        "Label": LABEL,
        "ProgramArguments": [str(python_path), str(sync_script(repo_root))],
        "WorkingDirectory": str(repo_root),
        "EnvironmentVariables": {"PYTHONUNBUFFERED": "1"},
        "RunAtLoad": True,
        "KeepAlive": True,
        "ThrottleInterval": 10,
        "StandardOutPath": str(home / "Library/Logs/codex-micro-title-sync.log"),
        "StandardErrorPath": str(
            home / "Library/Logs/codex-micro-title-sync.error.log"
        ),
    }


def build_windows_task_command(repo_root, python_path):
    return subprocess.list2cmdline(
        [str(python_path), str(sync_script(repo_root))]
    )
```

- [ ] **Step 4: Run pure-generation tests and verify GREEN**

Run:

```sh
PYTHONPATH=. python -m unittest -v tests/test_title_sync_service.py
```

Expected: both service-definition tests PASS.

- [ ] **Step 5: Commit portable service generation**

```sh
git add tools/title_sync_service.py tests/test_title_sync_service.py
git commit -m "generate portable title sync services"
```

### Task 3: Implement install, status, and uninstall

**Files:**
- Modify: `tools/title_sync_service.py`
- Modify: `tests/test_title_sync_service.py`

- [ ] **Step 1: Write failing command-construction and dispatch tests**

Add a recording runner:

```python
class RecordingRunner:
    def __init__(self, responses=None):
        self.calls = []
        self.responses = list(responses or [])

    def __call__(self, args, **kwargs):
        self.calls.append((list(map(str, args)), kwargs))
        return self.responses.pop(0) if self.responses else subprocess.CompletedProcess(
            args, 0, "", ""
        )
```

Test these public functions:

```python
def test_macos_install_replaces_only_own_agent(self):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        repo = root / "project"
        home = root / "home"
        python = repo / ".title-sync-venv/bin/python"
        (repo / "tools").mkdir(parents=True)
        python.parent.mkdir(parents=True)
        python.touch()
        (repo / "tools/sync_titles_usb.py").touch()
        runner = RecordingRunner()
        plist_path = install_macos(
            repo, python, home, uid=501, runner=runner
        )
        calls = [call[0] for call in runner.calls]
        self.assertIn(
            ["launchctl", "bootout", f"gui/501/{LABEL}"],
            calls,
        )
        self.assertIn(
            ["launchctl", "bootstrap", "gui/501", str(plist_path)],
            calls,
        )
        self.assertNotIn(
            "--port",
            plist_path.read_text(encoding="utf-8"),
        )

def test_windows_install_creates_current_user_logon_task(self):
    repo = PureWindowsPath(r"C:\Users\Example User\project")
    python = repo / r".title-sync-venv\Scripts\pythonw.exe"
    runner = RecordingRunner()
    install_windows(repo, python, runner=runner)
    create = runner.calls[0][0]
    self.assertEqual(create[:3], ["schtasks.exe", "/Create", "/F"])
    self.assertIn("/SC", create)
    self.assertIn("ONLOGON", create)
    self.assertIn("/RL", create)
    self.assertIn("LIMITED", create)
    self.assertIn(TASK_NAME, create)
    self.assertNotIn("SYSTEM", create)
    self.assertNotIn("/RP", create)

def test_uninstall_is_idempotent(self):
    with tempfile.TemporaryDirectory() as directory:
        runner = RecordingRunner()
        result = uninstall_macos(
            Path(directory), uid=501, runner=runner
        )
        self.assertEqual(result, "absent")

def test_dispatch_rejects_unsupported_platform(self):
    with self.assertRaisesRegex(RuntimeError, "macOS and Windows"):
        dispatch(
            "install",
            repo_root=Path("/tmp/example-project"),
            home=Path("/tmp/example-home"),
            platform="linux",
        )
```

- [ ] **Step 2: Run targeted tests and verify RED**

Run:

```sh
PYTHONPATH=. python -m unittest -v tests/test_title_sync_service.py
```

Expected: FAIL because install/status/uninstall functions are missing.

- [ ] **Step 3: Implement runtime bootstrap**

Add:

```python
def ensure_runtime(repo_root, platform=sys.platform, runner=subprocess.run):
    target = runtime_python(repo_root, platform)
    probe = [str(target), "-c", "import serial"]
    if not target.exists():
        venv.EnvBuilder(with_pip=True).create(Path(repo_root) / RUNTIME_DIR)
    result = runner(probe, capture_output=True, text=True)
    if result.returncode != 0:
        runner(
            [
                str(target),
                "-m",
                "pip",
                "install",
                "--disable-pip-version-check",
                PYSERIAL_REQUIREMENT,
            ],
            check=True,
        )
    return target
```

Use `python.exe` instead of `pythonw.exe` for the Windows import probe when
necessary, while registering `pythonw.exe` for the scheduled task.

- [ ] **Step 4: Implement macOS operations**

Add functions with injected `runner` for tests:

```python
def macos_plist_path(home):
    return Path(home) / "Library" / "LaunchAgents" / f"{LABEL}.plist"


def macos_domain(uid=None):
    return f"gui/{uid if uid is not None else os.getuid()}"


def install_macos(repo_root, python_path, home, uid=None, runner=subprocess.run):
    plist_path = macos_plist_path(home)
    plist_path.parent.mkdir(parents=True, exist_ok=True)
    (Path(home) / "Library" / "Logs").mkdir(parents=True, exist_ok=True)
    plist_path.write_bytes(
        plistlib.dumps(build_macos_plist(repo_root, python_path, home))
    )
    runner(["plutil", "-lint", str(plist_path)], check=True)
    target = f"{macos_domain(uid)}/{LABEL}"
    runner(["launchctl", "bootout", target], capture_output=True, text=True)
    runner(
        ["launchctl", "bootstrap", macos_domain(uid), str(plist_path)],
        check=True,
    )
    runner(["launchctl", "print", target], check=True)
    return plist_path
```

Implement `status_macos` with `launchctl print` and `uninstall_macos` with
`bootout` followed by unlinking only `macos_plist_path(home)`.

- [ ] **Step 5: Implement Windows operations**

Add:

```python
def install_windows(repo_root, python_path, runner=subprocess.run):
    command = build_windows_task_command(repo_root, python_path)
    runner(
        [
            "schtasks.exe", "/Create", "/F",
            "/TN", TASK_NAME,
            "/SC", "ONLOGON",
            "/RL", "LIMITED",
            "/TR", command,
        ],
        check=True,
    )
    runner(["schtasks.exe", "/Run", "/TN", TASK_NAME], check=True)
    runner(["schtasks.exe", "/Query", "/TN", TASK_NAME], check=True)
```

Implement `status_windows` with `/Query` and idempotent `uninstall_windows`
with `/Delete /F`.

- [ ] **Step 6: Implement CLI dispatch**

Add:

```python
def dispatch(action, repo_root=None, home=None, platform=sys.platform):
    repo_root = Path(repo_root or Path(__file__).resolve().parent.parent)
    home = Path(home or Path.home())
    if platform not in {"darwin", "win32"}:
        raise RuntimeError("Title sync service supports macOS and Windows only")
    if action == "install":
        python_path = ensure_runtime(repo_root, platform)
        return (
            install_macos(repo_root, python_path, home)
            if platform == "darwin"
            else install_windows(repo_root, python_path)
        )
    if action == "status":
        return status_macos(home) if platform == "darwin" else status_windows()
    return uninstall_macos(home) if platform == "darwin" else uninstall_windows()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("install", "status", "uninstall"))
    args = parser.parse_args()
    result = dispatch(args.action)
    print(result)


if __name__ == "__main__":
    main()
```

- [ ] **Step 7: Run the full unit suite and verify GREEN**

Run:

```sh
PYTHONPATH=. python -m unittest discover -v
```

Expected: all synchronizer and service-manager tests PASS.

- [ ] **Step 8: Commit service lifecycle support**

```sh
git add tools/title_sync_service.py tests/test_title_sync_service.py
git commit -m "manage title sync service lifecycle"
```

### Task 4: Document and package the unified installer

**Files:**
- Modify: `.gitignore`
- Modify: `README.md`
- Modify: `tests/test_title_sync_service.py`

- [ ] **Step 1: Write failing documentation and repository privacy tests**

Add:

```python
def test_readme_documents_unified_install_command(self):
    root = Path(__file__).resolve().parents[1]
    readme = (root / "README.md").read_text(encoding="utf-8")
    self.assertIn(
        "python tools/title_sync_service.py install",
        readme,
    )
    self.assertIn(
        "python tools/title_sync_service.py uninstall",
        readme,
    )


def test_tracked_service_sources_have_no_developer_paths(self):
    root = Path(__file__).resolve().parents[1]
    paths = [
        root / "tools/title_sync_service.py",
        root / "README.md",
        root / "docs/superpowers/specs/2026-07-28-cross-platform-title-sync-service-design.md",
    ]
    text = "\n".join(path.read_text(encoding="utf-8") for path in paths)
    self.assertNotIn("/Users/", text)
    self.assertNotIn("usbserial-", text)
    self.assertNotRegex(text, r"[A-Za-z]:\\\\Users\\\\(?!Example User)")
```

- [ ] **Step 2: Run the documentation test and verify RED**

Run:

```sh
PYTHONPATH=. python -m unittest -v \
  tests.test_title_sync_service.ServiceDefinitionTests.test_readme_documents_unified_install_command
```

Expected: FAIL because the README does not yet document the unified command.

- [ ] **Step 3: Ignore the generated runtime**

Add to `.gitignore`:

```gitignore
.title-sync-venv/
```

- [ ] **Step 4: Document unified commands and privacy behavior**

Replace platform-specific setup prose with:

```markdown
### One-command background service

From the repository root, use the same command on macOS and Windows:

```sh
python tools/title_sync_service.py install
python tools/title_sync_service.py status
python tools/title_sync_service.py uninstall
```

The installer creates `.title-sync-venv`, installs `pyserial==3.5`, detects the
current OS, and registers a current-user login service. It never requires
administrator privileges. Device ports and repository paths are resolved
locally and are not committed.

Background logs redact task titles. Run the foreground helper with
`--show-titles` only when title text is intentionally needed for debugging.
```

Also retain manual `sync_titles_usb.py` instructions for troubleshooting.

- [ ] **Step 5: Run privacy and full unit tests**

Run:

```sh
PYTHONPATH=. python -m unittest discover -v
git diff --check
```

Expected: all tests PASS and `git diff --check` prints nothing.

- [ ] **Step 6: Commit documentation and packaging**

```sh
git add .gitignore README.md tests/test_title_sync_service.py
git commit -m "document cross-platform title sync setup"
```

### Task 5: Validate the generated macOS service end to end

**Files:**
- No source changes expected.
- Generated local files remain outside Git.

- [ ] **Step 1: Record current service state**

Run:

```sh
python tools/title_sync_service.py status
```

Expected: existing or absent state is reported without exposing task titles.

- [ ] **Step 2: Install through the unified entrypoint**

Run:

```sh
python tools/title_sync_service.py install
```

Expected: runtime creation succeeds, launchd reports the stable label, and no
administrator prompt appears.

- [ ] **Step 3: Verify process and privacy-safe logs**

Run:

```sh
python tools/title_sync_service.py status
```

Expected: service is running.

Inspect the generated stdout and stderr logs. Expected: stdout reports a
connection and an assigned-slot count; neither log contains a task title.

- [ ] **Step 4: Verify serial acknowledgement**

With the StickC connected, wait for the service log to contain `Synced:` and
confirm it reports only an assigned count. Expected: the device acknowledges
the write and the service remains running.

- [ ] **Step 5: Verify unit suite and firmware non-change**

Run:

```sh
PYTHONPATH=. python -m unittest discover -v
git diff --check
git diff origin/main -- src include platformio.ini
```

Expected: all tests PASS, whitespace check is clean, and firmware diff is empty.

### Task 6: Publish the implementation

**Files:**
- Review all tracked changes.

- [ ] **Step 1: Inspect final scope**

Run:

```sh
git status -sb
git diff --stat origin/main
git diff --name-only origin/main
```

Expected: only host synchronizer, service manager, tests, docs, and ignore
rules are changed; no firmware path appears.

- [ ] **Step 2: Run final verification**

Run:

```sh
PYTHONPATH=. python -m unittest discover -v
python -m py_compile tools/sync_titles_usb.py tools/title_sync_service.py
git diff --check origin/main
```

Expected: all commands exit zero.

- [ ] **Step 3: Commit any remaining intended files**

```sh
git add .gitignore README.md tools/sync_titles_usb.py \
  tools/title_sync_service.py tests docs
git commit -m "add cross-platform title sync service"
```

If there is nothing left to commit, preserve the earlier focused commits.

- [ ] **Step 4: Push the branch**

```sh
git push -u origin codex/cross-platform-title-sync-service
```

Expected: remote tracking branch is created successfully.

- [ ] **Step 5: Open a draft pull request**

Create a draft PR targeting `main` with:

- Title: `Add cross-platform Codex Micro title sync service`
- Summary: unified macOS/Windows service installation, Recent/Custom title
  support, privacy-safe logging, tests, and documentation.
- Validation: unit suite, macOS launchd end-to-end sync, and empty firmware diff.
- Limitation: Windows Task Scheduler is unit-tested but still requires a
  physical Windows validation pass.
