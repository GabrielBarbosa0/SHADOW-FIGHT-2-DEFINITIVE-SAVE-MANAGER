from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from save_manager.adb_client import AdbClient, AdbError, PACKAGE, REMOTE_USERDATA


class DeviceBackupTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.backup_root = Path(self.temp.name)
        self.logs: list[str] = []
        self.client = AdbClient(adb="adb", log=self.logs.append)

    def test_backup_requires_installed_package(self) -> None:
        with patch.object(self.client, "package_installed", return_value=False):
            with self.assertRaisesRegex(AdbError, PACKAGE):
                self.client.backup("device-1", self.backup_root)

        self.assertEqual(list(self.backup_root.iterdir()), [])

    def test_backup_saves_and_validates_current_profile(self) -> None:
        def run(args, **_kwargs):
            if args[:2] == ["pull", REMOTE_USERDATA]:
                userdata = Path(args[2]) / "userdata"
                userdata.mkdir(parents=True)
                (userdata / "users.xml").write_text("<Root />", encoding="utf-8")
                (userdata / "users_backup.xml").write_text("<Root />", encoding="utf-8")
            return subprocess.CompletedProcess(args, 0, "", "")

        with (
            patch.object(self.client, "package_installed", return_value=True),
            patch.object(self.client, "_run", side_effect=run),
        ):
            destination = self.client.backup("device-1", self.backup_root)

        self.assertTrue((destination / "userdata" / "users.xml").is_file())
        self.assertTrue((destination / "userdata" / "users_backup.xml").is_file())
        self.assertTrue(any("Save atual confirmado" in message for message in self.logs))

    def test_backup_sanitizes_network_adb_serial_for_windows(self) -> None:
        def run(args, **_kwargs):
            if args[:2] == ["pull", REMOTE_USERDATA]:
                userdata = Path(args[2]) / "userdata"
                userdata.mkdir(parents=True)
                (userdata / "users.xml").write_text("<Root />", encoding="utf-8")
            return subprocess.CompletedProcess(args, 0, "", "")

        with (
            patch.object(self.client, "package_installed", return_value=True),
            patch.object(self.client, "_run", side_effect=run),
        ):
            destination = self.client.backup("127.0.0.1:5555", self.backup_root)

        self.assertTrue(destination.name.startswith("127.0.0.1_5555-"))
        self.assertNotIn(":", destination.name)

    def test_backup_rejects_copy_without_users_xml(self) -> None:
        def run(args, **_kwargs):
            if args[:2] == ["pull", REMOTE_USERDATA]:
                (Path(args[2]) / "userdata").mkdir(parents=True)
            return subprocess.CompletedProcess(args, 0, "", "")

        with (
            patch.object(self.client, "package_installed", return_value=True),
            patch.object(self.client, "_run", side_effect=run),
        ):
            with self.assertRaisesRegex(AdbError, "users.xml"):
                self.client.backup("device-1", self.backup_root)


if __name__ == "__main__":
    unittest.main()
