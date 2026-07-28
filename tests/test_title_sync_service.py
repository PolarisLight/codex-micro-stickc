import plistlib
import subprocess
import tempfile
import unittest
from pathlib import Path, PureWindowsPath
from unittest.mock import Mock

from tools import title_sync_service as service


class ServiceDefinitionTests(unittest.TestCase):
    def test_macos_plist_contains_runtime_paths_only(self):
        root = Path("/tmp/example-user/project")
        home = Path("/tmp/example-user")
        payload = plistlib.loads(
            service.build_macos_plist(
                root,
                root / ".title-sync-venv/bin/python",
                home,
            )
        )

        self.assertEqual(payload["Label"], service.SERVICE_ID)
        self.assertEqual(
            payload["ProgramArguments"],
            [
                "/tmp/example-user/project/.title-sync-venv/bin/python",
                "/tmp/example-user/project/tools/sync_titles_auto.py",
            ],
        )
        self.assertTrue(payload["RunAtLoad"])
        self.assertTrue(payload["KeepAlive"])

    def test_windows_task_command_handles_spaces(self):
        root = PureWindowsPath(r"C:\Users\Example User\codex micro")
        command = service.build_windows_task_command(
            root,
            root / ".title-sync-venv" / "Scripts" / "pythonw.exe",
        )

        self.assertIn(
            r'"C:\Users\Example User\codex micro\.title-sync-venv\Scripts\pythonw.exe"',
            command,
        )
        self.assertIn(
            r'"C:\Users\Example User\codex micro\tools\sync_titles_auto.py"',
            command,
        )

    def test_runtime_installs_usb_and_ble_dependencies(self):
        self.assertEqual(
            service.RUNTIME_REQUIREMENTS,
            ("pyserial==3.5", "bleak==3.0.2"),
        )

    def test_installer_requires_python_310_or_newer(self):
        with self.assertRaisesRegex(RuntimeError, "Python 3.10"):
            service.require_supported_python((3, 9))

        service.require_supported_python((3, 10))

    def test_runtime_probe_checks_version_and_dependencies(self):
        command = service.runtime_probe_command(Path("/tmp/runtime/python"))

        self.assertEqual(command[:2], ["/tmp/runtime/python", "-c"])
        self.assertIn("sys.version_info >= (3, 10)", command[2])
        self.assertIn("import serial, bleak", command[2])

    def test_runtime_uses_symlinks_for_dynamic_python_builds(self):
        factory = Mock()
        builder = factory.return_value

        service.create_runtime(
            Path("/tmp/runtime"),
            clear=True,
            platform="darwin",
            builder_factory=factory,
        )

        factory.assert_called_once_with(
            with_pip=True,
            clear=True,
            symlinks=True,
        )
        builder.create.assert_called_once_with(Path("/tmp/runtime"))

    def test_windows_runtime_does_not_require_symlink_privileges(self):
        factory = Mock()

        service.create_runtime(
            Path("/tmp/runtime"),
            platform="win32",
            builder_factory=factory,
        )

        factory.assert_called_once_with(
            with_pip=True,
            clear=False,
            symlinks=False,
        )

    def test_installer_copies_all_transport_helpers(self):
        self.assertEqual(
            service.HELPER_NAMES,
            (
                "sync_titles_auto.py",
                "sync_titles_ble.py",
                "sync_titles_usb.py",
            ),
        )

    def test_unsupported_platform_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "macOS and Windows"):
            service.platform_name("linux")

    def test_service_root_is_outside_protected_documents_on_macos(self):
        self.assertEqual(
            service.service_root("darwin", home=Path("/tmp/example-user")),
            Path(
                "/tmp/example-user/Library/Application Support/"
                "CodexMicroTitleSync"
            ),
        )


class ServiceOperationsTests(unittest.TestCase):
    @staticmethod
    def successful_run(*args, **kwargs):
        return subprocess.CompletedProcess(args[0], 0, "", "")

    def test_install_macos_registers_current_user_agent(self):
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory)
            root = home / "project"
            root.mkdir()
            runner = Mock(side_effect=self.successful_run)

            plist_path = service.install_macos(
                root,
                root / ".title-sync-venv/bin/python",
                home=home,
                uid=501,
                runner=runner,
            )

            self.assertTrue(plist_path.exists())
            commands = [call.args[0] for call in runner.call_args_list]
            self.assertIn(
                ["launchctl", "bootstrap", "gui/501", str(plist_path)],
                commands,
            )

    def test_install_windows_creates_limited_logon_task(self):
        root = PureWindowsPath(r"C:\Users\Example User\project")
        runner = Mock(side_effect=self.successful_run)

        service.install_windows(
            root,
            root / ".title-sync-venv" / "Scripts" / "pythonw.exe",
            runner=runner,
        )

        create = runner.call_args_list[0].args[0]
        self.assertEqual(create[:4], ["schtasks.exe", "/Create", "/F", "/TN"])
        self.assertIn("/RL", create)
        self.assertEqual(create[create.index("/RL") + 1], "LIMITED")
        self.assertEqual(create[create.index("/SC") + 1], "ONLOGON")


class DocumentationTests(unittest.TestCase):
    def test_readme_documents_unified_commands(self):
        readme = (Path(__file__).parents[1] / "README.md").read_text(
            encoding="utf-8"
        )
        for action in ("install", "status", "uninstall"):
            self.assertIn(
                f"python tools/title_sync_service.py {action}",
                readme,
            )

    def test_repository_files_do_not_contain_local_identifiers(self):
        root = Path(__file__).parents[1]
        forbidden = (
            "/Users/" + "polaris",
            "855287" + "DA00",
            "com." + "polaris.",
        )
        paths = [
            root / ".gitignore",
            root / "README.md",
            *sorted((root / "tools").glob("*.py")),
            *sorted((root / "tests").glob("*.py")),
        ]
        for path in paths:
            text = path.read_text(encoding="utf-8")
            for value in forbidden:
                self.assertNotIn(value, text, str(path))


if __name__ == "__main__":
    unittest.main()
