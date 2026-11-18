from __future__ import annotations

import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk
from typing import Callable

from save_manager.adb_client import AdbClient, Device
from save_manager.save_model import (
    CHEAT_GEMS,
    CHEAT_MONEY,
    CHEAT_ORBS,
    MAX_LEVEL,
    SaveConfig,
    SaveSummary,
    generate_save,
)


ROOT = Path(__file__).resolve().parents[1]
FRESH_TEMPLATE = ROOT / "saves" / "templates" / "fresh-default.xml"
TITAN_TEMPLATE = ROOT / "saves" / "titan-complete" / "users.xml"
PROGRESS_TEMPLATE = ROOT / "saves" / "templates" / "character-progress.xml"
GENERATED_SAVE = ROOT / "generated" / "users.xml"
BACKUP_ROOT = ROOT / "backups"

BASE_LABELS = {
    "Campanha nova": "fresh",
    "Save completo original": "titan_complete",
}
EQUIPMENT_LABELS = {
    "Shadow inicial": "initial",
    "Conjunto de Lynx": "lynx",
    "Conjunto do Eremita": "hermit",
    "Conjunto do Açougueiro": "butcher",
    "Conjunto da Vespa": "wasp",
    "Conjunto da Viúva": "widow",
    "Conjunto do Shogun": "shogun",
    "Equipamento do Titã": "titan",
}

PRESETS = {
    "Campanha normal": dict(base="Campanha nova", level=1, money=False, gems=False, energy=False, orbs=False, equipment="Shadow inicial"),
    "Campanha rica": dict(base="Campanha nova", level=1, money=True, gems=True, energy=True, orbs=False, equipment="Shadow inicial"),
    "Campanha máxima": dict(base="Campanha nova", level=52, money=True, gems=True, energy=True, orbs=True, equipment="Shadow inicial"),
    "Titã no início": dict(base="Campanha nova", level=52, money=True, gems=True, energy=True, orbs=True, equipment="Equipamento do Titã"),
    "Save completo original": dict(base="Save completo original", level=52, money=True, gems=True, energy=True, orbs=True, equipment="Equipamento do Titã"),
}


class SaveManagerApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Shadow Fight 2: Definitive Edition — Gerenciador de Save")
        self.geometry("1000x790")
        self.minsize(900, 720)

        self._messages: queue.Queue[str] = queue.Queue()
        self._devices: dict[str, Device] = {}
        self._last_summary: SaveSummary | None = None
        self.adb = AdbClient(log=self._queue_log)

        self._create_variables()
        self._configure_style()
        self._build_ui()
        self.after(100, self._drain_log)
        self.after(250, self.refresh_devices)
        self.apply_preset()

    def _create_variables(self) -> None:
        self.preset_var = tk.StringVar(value="Campanha máxima")
        self.base_var = tk.StringVar(value="Campanha nova")
        self.level_var = tk.StringVar(value="52")
        self.money_enabled_var = tk.BooleanVar(value=True)
        self.money_var = tk.StringVar(value=str(CHEAT_MONEY))
        self.gems_enabled_var = tk.BooleanVar(value=True)
        self.gems_var = tk.StringVar(value=str(CHEAT_GEMS))
        self.energy_var = tk.BooleanVar(value=True)
        self.orbs_enabled_var = tk.BooleanVar(value=True)
        self.orbs_var = tk.StringVar(value=str(CHEAT_ORBS))
        self.equipment_var = tk.StringVar(value="Shadow inicial")
        self.device_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Preparando interface...")

    def _configure_style(self) -> None:
        style = ttk.Style(self)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Segoe UI", 18, "bold"))
        style.configure("Subtitle.TLabel", font=("Segoe UI", 10), foreground="#555555")
        style.configure("Section.TLabelframe.Label", font=("Segoe UI", 10, "bold"))
        style.configure("Accent.TButton", font=("Segoe UI", 10, "bold"))
        style.configure("Action.TButton", font=("Segoe UI", 10, "bold"), padding=(14, 7))

    def _build_ui(self) -> None:
        outer = ttk.Frame(self, padding=16)
        outer.pack(fill="both", expand=True)

        ttk.Label(
            outer,
            text="Gerenciador de Save — Shadow Fight 2: Definitive Edition",
            style="Title.TLabel",
        ).pack(anchor="w")
        ttk.Label(
            outer,
            text="Configure um perfil, confira o resumo e aplique no Android com backup automático.",
            style="Subtitle.TLabel",
        ).pack(anchor="w", pady=(2, 12))

        preset_bar = ttk.Frame(outer)
        preset_bar.pack(fill="x", pady=(0, 10))
        ttk.Label(preset_bar, text="Preset:").pack(side="left")
        preset_combo = ttk.Combobox(
            preset_bar,
            textvariable=self.preset_var,
            values=list(PRESETS),
            state="readonly",
            width=28,
        )
        preset_combo.pack(side="left", padx=8)
        preset_combo.bind("<<ComboboxSelected>>", lambda _event: self.apply_preset())
        ttk.Button(preset_bar, text="Aplicar preset", command=self.apply_preset).pack(side="left")

        content = ttk.Frame(outer)
        content.pack(fill="both", expand=True)
        content.columnconfigure(0, weight=1)
        content.columnconfigure(1, weight=1)
        content.rowconfigure(1, weight=1)

        campaign = ttk.LabelFrame(content, text=" Campanha e nível ", style="Section.TLabelframe", padding=12)
        campaign.grid(row=0, column=0, sticky="nsew", padx=(0, 6), pady=(0, 8))
        campaign.columnconfigure(1, weight=1)
        ttk.Label(campaign, text="Perfil-base:").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Combobox(
            campaign,
            textvariable=self.base_var,
            values=list(BASE_LABELS),
            state="readonly",
        ).grid(row=0, column=1, sticky="ew", padx=(8, 0), pady=4)
        ttk.Label(campaign, text="Nível (1–52):").grid(row=1, column=0, sticky="w", pady=4)
        ttk.Spinbox(campaign, from_=1, to=MAX_LEVEL, textvariable=self.level_var, width=10).grid(
            row=1, column=1, sticky="w", padx=(8, 0), pady=4
        )
        ttk.Label(
            campaign,
            text="Campanha nova mantém tutorial e Ato 1. O save completo mantém mapas e batalhas do perfil do Titã.",
            wraplength=410,
            style="Subtitle.TLabel",
        ).grid(row=2, column=0, columnspan=2, sticky="w", pady=(8, 0))

        equipment = ttk.LabelFrame(content, text=" Aparência e equipamento ", style="Section.TLabelframe", padding=12)
        equipment.grid(row=0, column=1, sticky="nsew", padx=(6, 0), pady=(0, 8))
        equipment.columnconfigure(1, weight=1)
        ttk.Label(equipment, text="Conjunto:").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Combobox(
            equipment,
            textvariable=self.equipment_var,
            values=list(EQUIPMENT_LABELS),
            state="readonly",
        ).grid(row=0, column=1, sticky="ew", padx=(8, 0), pady=4)
        ttk.Label(
            equipment,
            text="Conjuntos oficiais dos seis chefes principais e do Titã. A campanha continua controlável pelo jogador.",
            wraplength=410,
            style="Subtitle.TLabel",
        ).grid(row=1, column=0, columnspan=2, sticky="w", pady=(8, 0))

        resources = ttk.LabelFrame(content, text=" Recursos ", style="Section.TLabelframe", padding=12)
        resources.grid(row=1, column=0, sticky="nsew", padx=(0, 6), pady=(0, 8))
        resources.columnconfigure(1, weight=1)
        self._resource_row(resources, 0, "Moedas elevadas", self.money_enabled_var, self.money_var)
        self._resource_row(resources, 1, "Gemas elevadas", self.gems_enabled_var, self.gems_var)
        ttk.Checkbutton(resources, text="Energia infinita", variable=self.energy_var).grid(
            row=2, column=0, columnspan=2, sticky="w", pady=6
        )
        self._resource_row(resources, 3, "Orbes elevados", self.orbs_enabled_var, self.orbs_var)
        ttk.Label(
            resources,
            text="Desmarcado: moedas 0, gemas 50 e orbes 0. Os três tipos de orbe usam o mesmo valor nesta versão.",
            wraplength=410,
            style="Subtitle.TLabel",
        ).grid(row=4, column=0, columnspan=2, sticky="w", pady=(8, 0))

        device = ttk.LabelFrame(content, text=" Android / ADB ", style="Section.TLabelframe", padding=12)
        device.grid(row=1, column=1, sticky="nsew", padx=(6, 0), pady=(0, 8))
        device.columnconfigure(0, weight=1)
        ttk.Label(device, text="Aparelho conectado:").grid(row=0, column=0, sticky="w")
        self.device_combo = ttk.Combobox(device, textvariable=self.device_var, state="readonly")
        self.device_combo.grid(row=1, column=0, sticky="ew", pady=(4, 8))
        device_buttons = ttk.Frame(device)
        device_buttons.grid(row=2, column=0, sticky="ew")
        self.refresh_button = ttk.Button(device_buttons, text="Atualizar aparelhos", command=self.refresh_devices)
        self.refresh_button.pack(side="left")
        ttk.Label(
            device,
            text="Aplicar cria um backup local antes de substituir users.xml e users_backup.xml.",
            wraplength=410,
            style="Subtitle.TLabel",
        ).grid(row=3, column=0, sticky="w", pady=(10, 0))

        actions = ttk.Frame(outer)
        actions.pack(fill="x", pady=(4, 8))
        self.generate_button = ttk.Button(
            actions,
            text="GERAR SAVE",
            style="Action.TButton",
            width=18,
            command=self.generate,
        )
        self.generate_button.pack(side="left")
        self.backup_button = ttk.Button(
            actions,
            text="FAZER BACKUP",
            style="Action.TButton",
            width=18,
            command=self.backup_device,
        )
        self.backup_button.pack(side="left", padx=8)
        self.apply_button = ttk.Button(
            actions,
            text="APLICAR SAVE",
            style="Action.TButton",
            width=18,
            command=self.apply_to_device,
        )
        self.apply_button.pack(side="left", padx=8)
        ttk.Label(actions, textvariable=self.status_var).pack(side="right")

        notebook = ttk.Notebook(outer)
        notebook.pack(fill="both", expand=True)
        preview_frame = ttk.Frame(notebook, padding=8)
        log_frame = ttk.Frame(notebook, padding=8)
        notebook.add(preview_frame, text="Resumo do save")
        notebook.add(log_frame, text="Log ADB")
        self.preview_text = tk.Text(preview_frame, height=10, wrap="word", font=("Consolas", 10), state="disabled")
        self.preview_text.pack(fill="both", expand=True)
        self.log_text = tk.Text(log_frame, height=10, wrap="word", font=("Consolas", 9), state="disabled")
        self.log_text.pack(fill="both", expand=True)

    def _resource_row(
        self,
        parent: ttk.LabelFrame,
        row: int,
        text: str,
        enabled_var: tk.BooleanVar,
        value_var: tk.StringVar,
    ) -> None:
        ttk.Checkbutton(parent, text=text, variable=enabled_var).grid(row=row, column=0, sticky="w", pady=5)
        ttk.Entry(parent, textvariable=value_var).grid(row=row, column=1, sticky="ew", padx=(10, 0), pady=5)

    def apply_preset(self) -> None:
        preset = PRESETS[self.preset_var.get()]
        self.base_var.set(preset["base"])
        self.level_var.set(str(preset["level"]))
        self.money_enabled_var.set(preset["money"])
        self.gems_enabled_var.set(preset["gems"])
        self.energy_var.set(preset["energy"])
        self.orbs_enabled_var.set(preset["orbs"])
        self.equipment_var.set(preset["equipment"])
        self.status_var.set(f"Preset carregado: {self.preset_var.get()}")

    def _read_nonnegative_int(self, label: str, value: str) -> int:
        try:
            parsed = int(value.replace(".", "").replace(" ", ""))
        except ValueError as exc:
            raise ValueError(f"{label} deve ser um número inteiro.") from exc
        if parsed < 0:
            raise ValueError(f"{label} não pode ser negativo.")
        return parsed

    def _read_config(self) -> SaveConfig:
        level = self._read_nonnegative_int("Nível", self.level_var.get())
        return SaveConfig(
            base_profile=BASE_LABELS[self.base_var.get()],
            level=level,
            rich_money=self.money_enabled_var.get(),
            money=self._read_nonnegative_int("Moedas", self.money_var.get()),
            rich_gems=self.gems_enabled_var.get(),
            gems=self._read_nonnegative_int("Gemas", self.gems_var.get()),
            unlimited_energy=self.energy_var.get(),
            rich_orbs=self.orbs_enabled_var.get(),
            orbs=self._read_nonnegative_int("Orbes", self.orbs_var.get()),
            equipment=EQUIPMENT_LABELS[self.equipment_var.get()],
        )

    def generate(self, *, show_dialog: bool = True) -> SaveSummary | None:
        try:
            summary = generate_save(
                self._read_config(),
                FRESH_TEMPLATE,
                TITAN_TEMPLATE,
                PROGRESS_TEMPLATE,
                GENERATED_SAVE,
            )
        except Exception as exc:
            messagebox.showerror("Não foi possível gerar o save", str(exc), parent=self)
            return None
        self._last_summary = summary
        self._set_text(self.preview_text, f"Arquivo: {summary.path}\n\n{summary.as_text()}")
        self.status_var.set("Save gerado e validado.")
        if show_dialog:
            messagebox.showinfo("Save gerado", f"Save salvo em:\n{summary.path}", parent=self)
        return summary

    def refresh_devices(self) -> None:
        self.status_var.set("Procurando aparelhos...")

        def done(devices: list[Device]) -> None:
            self._devices = {device.label: device for device in devices}
            labels = list(self._devices)
            self.device_combo.configure(values=labels)
            if labels:
                self.device_var.set(labels[0])
                self.status_var.set(f"{len(labels)} aparelho(s) conectado(s).")
            else:
                self.device_var.set("")
                self.status_var.set("Nenhum aparelho ADB autorizado.")

        self._background(self.adb.devices, done)

    def _selected_device(self) -> Device:
        device = self._devices.get(self.device_var.get())
        if device is None:
            raise ValueError("Selecione um aparelho ADB autorizado.")
        return device

    def backup_device(self) -> None:
        try:
            device = self._selected_device()
        except ValueError as exc:
            messagebox.showwarning("Aparelho necessário", str(exc), parent=self)
            return
        self.status_var.set("Criando backup...")

        def done(path: Path) -> None:
            self.status_var.set("Backup concluído.")
            messagebox.showinfo("Backup concluído", f"Backup salvo em:\n{path}", parent=self)

        self._background(lambda: self.adb.backup(device.serial, BACKUP_ROOT), done)

    def apply_to_device(self) -> None:
        summary = self.generate(show_dialog=False)
        if summary is None:
            return
        try:
            device = self._selected_device()
        except ValueError as exc:
            messagebox.showwarning("Aparelho necessário", str(exc), parent=self)
            return
        if not messagebox.askyesno(
            "Aplicar save",
            f"Aplicar o save no aparelho {device.description}?\n\nUm backup será criado antes da substituição.",
            parent=self,
        ):
            return
        self.status_var.set("Fazendo backup e aplicando save...")

        def done(backup_path: Path) -> None:
            self.status_var.set("Save aplicado e jogo iniciado.")
            messagebox.showinfo(
                "Save aplicado",
                f"O jogo foi iniciado no aparelho.\n\nBackup anterior:\n{backup_path}",
                parent=self,
            )

        self._background(lambda: self.adb.apply_save(device.serial, summary.path, BACKUP_ROOT), done)

    def _background(self, work: Callable[[], object], done: Callable[[object], None]) -> None:
        self._set_busy(True)

        def runner() -> None:
            try:
                result = work()
            except Exception as exc:
                self.after(0, lambda error=exc: self._show_background_error(error))
            else:
                self.after(0, lambda value=result: done(value))
                self.after(0, lambda: self._set_busy(False))

        threading.Thread(target=runner, daemon=True).start()

    def _show_background_error(self, exc: Exception) -> None:
        self._set_busy(False)
        self.status_var.set("Operação falhou.")
        messagebox.showerror("Erro", str(exc), parent=self)

    def _set_busy(self, busy: bool) -> None:
        state = "disabled" if busy else "normal"
        for button in (self.refresh_button, self.backup_button, self.generate_button, self.apply_button):
            button.configure(state=state)

    def _queue_log(self, message: str) -> None:
        self._messages.put(message)

    def _drain_log(self) -> None:
        messages: list[str] = []
        while True:
            try:
                messages.append(self._messages.get_nowait())
            except queue.Empty:
                break
        if messages:
            self.log_text.configure(state="normal")
            self.log_text.insert("end", "\n".join(messages) + "\n")
            self.log_text.see("end")
            self.log_text.configure(state="disabled")
        self.after(100, self._drain_log)

    @staticmethod
    def _set_text(widget: tk.Text, value: str) -> None:
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.insert("1.0", value)
        widget.configure(state="disabled")


def main() -> None:
    app = SaveManagerApp()
    app.mainloop()


if __name__ == "__main__":
    main()
