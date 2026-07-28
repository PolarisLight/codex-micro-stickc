#!/usr/bin/env python3
"""Install or manage Codex Micro title sync for the current user."""
from __future__ import annotations

import argparse
import os
import plistlib
import shutil
import subprocess
import sys
import venv
from pathlib import Path, PurePath

SERVICE_ID = "io.github.codex-micro-stickc.title-sync"
WINDOWS_TASK_NAME = "Codex Micro Title Sync"
PYSERIAL_REQUIREMENT = "pyserial==3.5"
RUNTIME_DIR = ".title-sync-venv"
INSTALL_DIR = "CodexMicroTitleSync"


def platform_name(value=sys.platform):
    if value == "darwin":
        return "macOS"
    if value == "win32":
        return "Windows"
    raise RuntimeError("The background service supports macOS and Windows only.")


def runtime_python(root: PurePath, platform=sys.platform, background=False):
    if platform == "win32":
        name = "pythonw.exe" if background else "python.exe"
        return root / RUNTIME_DIR / "Scripts" / name
    return root / RUNTIME_DIR / "bin" / "python"


def ensure_runtime(root: Path, platform=sys.platform, runner=subprocess.run):
    console = Path(runtime_python(root, platform))
    if not console.exists():
        venv.EnvBuilder(with_pip=True).create(root / RUNTIME_DIR)

    try:
        probe = runner(
            [str(console), "-c", "import serial"],
            capture_output=True,
            text=True,
        )
    except OSError:
        venv.EnvBuilder(with_pip=True, clear=True).create(root / RUNTIME_DIR)
        probe = runner(
            [str(console), "-c", "import serial"],
            capture_output=True,
            text=True,
        )
    if probe.returncode != 0:
        runner(
            [
                str(console),
                "-m",
                "pip",
                "install",
                "--disable-pip-version-check",
                PYSERIAL_REQUIREMENT,
            ],
            check=True,
        )
    return Path(runtime_python(root, platform, background=True))


def service_root(platform=sys.platform, home=None, environ=None):
    if platform == "darwin":
        home = Path.home() if home is None else Path(home)
        return home / "Library" / "Application Support" / INSTALL_DIR
    if platform == "win32":
        environ = os.environ if environ is None else environ
        local_app_data = environ.get("LOCALAPPDATA")
        if not local_app_data:
            raise RuntimeError("LOCALAPPDATA is not available.")
        return Path(local_app_data) / INSTALL_DIR
    platform_name(platform)


def copy_helper(project_root: Path, installed_root: Path):
    destination = installed_root / "tools" / "sync_titles_usb.py"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(project_root / "tools" / "sync_titles_usb.py", destination)
    return destination


def build_macos_plist(root: PurePath, python: PurePath, home: PurePath):
    logs = home / "Library" / "Logs"
    payload = {
        "Label": SERVICE_ID,
        "ProgramArguments": [
            str(python),
            str(root / "tools" / "sync_titles_usb.py"),
        ],
        "RunAtLoad": True,
        "KeepAlive": True,
        "ProcessType": "Background",
        "StandardOutPath": str(logs / f"{SERVICE_ID}.log"),
        "StandardErrorPath": str(logs / f"{SERVICE_ID}.error.log"),
        "EnvironmentVariables": {"PYTHONUNBUFFERED": "1"},
    }
    return plistlib.dumps(payload, fmt=plistlib.FMT_XML, sort_keys=True)


def macos_plist_path(home: PurePath):
    return home / "Library" / "LaunchAgents" / f"{SERVICE_ID}.plist"


def install_macos(root, python, home=None, uid=None, runner=subprocess.run):
    home = Path.home() if home is None else Path(home)
    uid = os.getuid() if uid is None else uid
    path = Path(macos_plist_path(home))
    path.parent.mkdir(parents=True, exist_ok=True)
    (home / "Library" / "Logs").mkdir(parents=True, exist_ok=True)
    path.write_bytes(build_macos_plist(root, python, home))

    domain = f"gui/{uid}"
    runner(
        ["launchctl", "bootout", f"{domain}/{SERVICE_ID}"],
        capture_output=True,
        text=True,
    )
    runner(["launchctl", "bootstrap", domain, str(path)], check=True)
    runner(["launchctl", "kickstart", "-k", f"{domain}/{SERVICE_ID}"], check=True)
    return path


def status_macos(uid=None, runner=subprocess.run):
    uid = os.getuid() if uid is None else uid
    result = runner(
        ["launchctl", "print", f"gui/{uid}/{SERVICE_ID}"],
        capture_output=True,
        text=True,
    )
    return "running" if result.returncode == 0 else "not installed"


def uninstall_macos(home=None, uid=None, runner=subprocess.run):
    home = Path.home() if home is None else Path(home)
    uid = os.getuid() if uid is None else uid
    path = Path(macos_plist_path(home))
    result = runner(
        ["launchctl", "bootout", f"gui/{uid}/{SERVICE_ID}"],
        capture_output=True,
        text=True,
    )
    existed = path.exists() or result.returncode == 0
    if path.exists():
        path.unlink()
    return "removed" if existed else "not installed"


def build_windows_task_command(root: PurePath, python: PurePath):
    return subprocess.list2cmdline(
        [str(python), str(root / "tools" / "sync_titles_usb.py")]
    )


def install_windows(root, python, runner=subprocess.run):
    command = build_windows_task_command(root, python)
    runner(
        [
            "schtasks.exe",
            "/Create",
            "/F",
            "/TN",
            WINDOWS_TASK_NAME,
            "/TR",
            command,
            "/SC",
            "ONLOGON",
            "/RL",
            "LIMITED",
        ],
        check=True,
    )
    runner(
        ["schtasks.exe", "/Run", "/TN", WINDOWS_TASK_NAME],
        check=True,
    )


def status_windows(runner=subprocess.run):
    result = runner(
        ["schtasks.exe", "/Query", "/TN", WINDOWS_TASK_NAME],
        capture_output=True,
        text=True,
    )
    return "installed" if result.returncode == 0 else "not installed"


def uninstall_windows(runner=subprocess.run):
    result = runner(
        ["schtasks.exe", "/Delete", "/F", "/TN", WINDOWS_TASK_NAME],
        capture_output=True,
        text=True,
    )
    return "removed" if result.returncode == 0 else "not installed"


def dispatch(action, root=None, platform=sys.platform):
    root = Path(__file__).resolve().parents[1] if root is None else Path(root)
    name = platform_name(platform)

    if action == "install":
        installed_root = service_root(platform)
        copy_helper(root, installed_root)
        python = ensure_runtime(installed_root, platform)
        if platform == "darwin":
            install_macos(installed_root, python)
        else:
            install_windows(installed_root, python)
        print(f"{name} title sync installed and started.")
    elif action == "status":
        status = status_macos() if platform == "darwin" else status_windows()
        print(f"{name} title sync: {status}.")
    elif platform == "darwin":
        print(f"{name} title sync: {uninstall_macos()}.")
    else:
        print(f"{name} title sync: {uninstall_windows()}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("install", "status", "uninstall"))
    args = parser.parse_args()
    try:
        dispatch(args.action)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f"title-sync service error: {error}\n")


if __name__ == "__main__":
    main()
