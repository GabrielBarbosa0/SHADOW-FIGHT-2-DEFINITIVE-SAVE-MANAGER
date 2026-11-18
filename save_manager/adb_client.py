from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Callable, Sequence


PACKAGE = "com.sf2.de"
REMOTE_USERDATA = f"/sdcard/Android/data/{PACKAGE}/files/userdata"
LogCallback = Callable[[str], None]


class AdbError(RuntimeError):
    pass


@dataclass(frozen=True)
class Device:
    serial: str
    description: str

    @property
    def label(self) -> str:
        return f"{self.serial} — {self.description}"


class AdbClient:
    def __init__(self, adb: str | None = None, log: LogCallback | None = None) -> None:
        self.adb = adb or shutil.which("adb") or "adb"
        self.log = log or (lambda _message: None)

    def _run(
        self,
        args: Sequence[str],
        *,
        serial: str | None = None,
        check: bool = True,
        timeout: int = 180,
    ) -> subprocess.CompletedProcess[str]:
        command = [self.adb]
        if serial:
            command.extend(["-s", serial])
        command.extend(args)
        self.log("> " + " ".join(command))
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except FileNotFoundError as exc:
            raise AdbError("adb não foi encontrado no PATH.") from exc
        except subprocess.TimeoutExpired as exc:
            raise AdbError("O comando ADB excedeu o tempo limite.") from exc
        output = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
        if output:
            self.log(output)
        if check and result.returncode != 0:
            raise AdbError(output or f"ADB terminou com código {result.returncode}.")
        return result

    def devices(self) -> list[Device]:
        result = self._run(["devices", "-l"])
        devices: list[Device] = []
        for raw in result.stdout.splitlines()[1:]:
            line = raw.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) < 2 or parts[1] != "device":
                continue
            details = {key: value for token in parts[2:] if ":" in token for key, value in [token.split(":", 1)]}
            model = details.get("model", "Android")
            product = details.get("product", "")
            description = model if not product else f"{model} ({product})"
            devices.append(Device(parts[0], description))
        return devices

    def package_installed(self, serial: str) -> bool:
        result = self._run(["shell", "pm", "path", PACKAGE], serial=serial, check=False)
        return result.returncode == 0 and "package:" in result.stdout

    def backup(self, serial: str, backup_root: Path) -> Path:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        destination = backup_root / f"{serial}-{stamp}"
        destination.mkdir(parents=True, exist_ok=False)
        self.log(f"Criando backup em {destination}")
        self._run(["shell", "am", "force-stop", PACKAGE], serial=serial)
        self._run(["pull", REMOTE_USERDATA, str(destination)], serial=serial, timeout=300)
        return destination

    def apply_save(self, serial: str, save_path: Path, backup_root: Path) -> Path:
        if not self.package_installed(serial):
            raise AdbError(f"O pacote {PACKAGE} não está instalado no aparelho selecionado.")
        backup_path = self.backup(serial, backup_root)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        self._run(["shell", "am", "force-stop", PACKAGE], serial=serial)
        for name in ("users.xml.hash", "users_backup.xml.hash"):
            source = f"{REMOTE_USERDATA}/{name}"
            destination = f"{source}.pre-import-{stamp}"
            self._run(["shell", "mv", source, destination], serial=serial, check=False)
        self._run(["push", str(save_path), f"{REMOTE_USERDATA}/users.xml"], serial=serial)
        self._run(["push", str(save_path), f"{REMOTE_USERDATA}/users_backup.xml"], serial=serial)
        self._run(
            ["shell", "monkey", "-p", PACKAGE, "-c", "android.intent.category.LAUNCHER", "1"],
            serial=serial,
        )
        return backup_path
