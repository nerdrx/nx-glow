#!/usr/bin/env python3
"""Small configuration window for the nx glow KWin effect."""
import argparse
import math
import os
import re
import subprocess
import sys
from pathlib import Path

from PySide6.QtCore import QProcess, QSignalBlocker, QTimer, Qt
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QSlider,
    QTextEdit,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

CONFIG_FILE = "kwinrc"
EFFECT_GROUP = "Effect-nxglow"
DEFAULTS = {"Radius": 110, "Strength": 85, "Saturation": 100}
DRY_RUN = os.environ.get("NX_GLOW_SETTINGS_DRY_RUN", "").lower() in {"1", "true", "yes"}


def read_config(group, key, default):
    result = subprocess.run(
        ["kreadconfig6", "--file", CONFIG_FILE, "--group", group,
         "--key", key, "--default", str(default)],
        check=True, capture_output=True, text=True, timeout=3,
    )
    return result.stdout.strip()


def write_config(group, key, value):
    subprocess.run(
        ["kwriteconfig6", "--file", CONFIG_FILE, "--group", group,
         "--key", key, str(value)],
        check=True, capture_output=True, text=True, timeout=3,
    )


def call_effect(method):
    result = subprocess.run(
        ["qdbus6", "org.kde.KWin", "/Effects",
         f"org.kde.kwin.Effects.{method}", "nxglow"],
        check=True, capture_output=True, text=True, timeout=5,
    )
    value = result.stdout.strip().lower()
    if value not in {"true", "false"}:
        raise RuntimeError(f"KWin returned unexpected result: {value or '(empty)'}")
    return value == "true"


def effect_is_loaded():
    result = subprocess.run(
        ["qdbus6", "org.kde.KWin", "/Effects",
         "org.kde.kwin.Effects.loadedEffects"],
        check=True, capture_output=True, text=True, timeout=5,
    )
    return re.search(r"(?<![\w])nxglow(?![\w])", result.stdout) is not None


class SettingsWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("nx glow settings")
        self.setMinimumWidth(390)
        self.resize(470, 430)
        self._values = DEFAULTS.copy()
        self.source_dir = Path(os.environ.get("NX_GLOW_SOURCE_DIR", Path(__file__).resolve().parent)).resolve()
        self._loading = True
        self._debounce = QTimer(self)
        self._debounce.setSingleShot(True)
        self._debounce.timeout.connect(self._apply_values)
        self._enabled = False
        self._setup_process = QProcess(self)
        self._setup_process.readyReadStandardOutput.connect(lambda: self._read_setup_output(False))
        self._setup_process.readyReadStandardError.connect(lambda: self._read_setup_output(True))
        self._setup_process.finished.connect(self._setup_finished)
        self._setup_process.errorOccurred.connect(self._setup_error)
        self._build_ui()
        self._load_config()

    def _build_ui(self):
        body = QWidget()
        layout = QVBoxLayout(body)
        layout.setContentsMargins(28, 25, 28, 24)
        layout.setSpacing(17)
        self.setCentralWidget(body)

        title = QLabel("nx glow")
        title.setObjectName("title")
        subtitle = QLabel("Shape the light around your desktop.")
        subtitle.setObjectName("subtitle")
        layout.addWidget(title)
        layout.addWidget(subtitle)

        self.enabled = QCheckBox("Enable desktop glow")
        self.enabled.setObjectName("enabled")
        self.enabled.toggled.connect(self._toggle_effect)
        enable_row = QHBoxLayout()
        enable_row.addWidget(self.enabled, 1)
        self.setup_button = QPushButton("Install / repair effect")
        self.setup_button.clicked.connect(self._start_setup)
        self._installer = self.source_dir / "install.sh"
        self.setup_button.setVisible(self._installer.is_file())
        enable_row.addWidget(self.setup_button)
        layout.addLayout(enable_row)

        self.controls = {}
        for key, title_text, maximum in (
            ("Radius", "Glow size", 300),
            ("Strength", "Brightness", 200),
            ("Saturation", "Colour intensity", 200),
        ):
            layout.addWidget(self._slider_row(key, title_text, maximum))

        info = QLabel("Changes apply live and save automatically.")
        info.setObjectName("hint")
        info.setWordWrap(True)
        layout.addWidget(info)

        bottom = QHBoxLayout()
        self.status = QLabel("Loading settings…")
        self.status.setObjectName("status")
        self.status.setWordWrap(True)
        self.reset = QPushButton("Reset defaults")
        self.reset.clicked.connect(self._reset)
        bottom.addWidget(self.status, 1)
        bottom.addWidget(self.reset)
        layout.addLayout(bottom)
        self.details_button = QToolButton()
        self.details_button.setText("Show setup details")
        self.details_button.setCheckable(True)
        self.details_button.toggled.connect(self._toggle_details)
        layout.addWidget(self.details_button)
        self.details = QTextEdit()
        self.details.setReadOnly(True)
        self.details.setMaximumHeight(125)
        self.details.hide()
        layout.addWidget(self.details)
        self.setStyleSheet("""
            QMainWindow { background: #11131c; }
            QWidget { color: #f0eff8; font-size: 13px; }
            QLabel#title { font-size: 27px; font-weight: 700; letter-spacing: -1px; }
            QLabel#subtitle, QLabel#hint, QLabel#status { color: #a6a8b8; }
            QLabel#hint { padding: 12px; border: 1px solid rgba(255,255,255,26); border-radius: 9px; }
            QCheckBox { font-size: 15px; font-weight: 600; spacing: 10px; }
            QCheckBox::indicator { width: 18px; height: 18px; }
            QSlider::groove:horizontal { height: 5px; background: #333543; border-radius: 3px; }
            QSlider::sub-page:horizontal { background: #a486ff; border-radius: 3px; }
            QSlider::handle:horizontal { width: 15px; margin: -6px 0; border-radius: 8px; background: #e5dcff; }
            QPushButton, QToolButton { padding: 8px 12px; border: 1px solid rgba(255,255,255,37); border-radius: 7px; background: #1a1d29; }
            QPushButton:hover, QToolButton:hover { background: #292c3a; }
            QToolButton:disabled { color: #8c8e9e; }
        """)

    def _slider_row(self, key, title, maximum):
        frame = QFrame()
        row = QVBoxLayout(frame)
        row.setContentsMargins(0, 2, 0, 0)
        row.setSpacing(6)
        header = QHBoxLayout()
        name = QLabel(title)
        value = QLabel()
        value.setObjectName("value")
        value.setAlignment(Qt.AlignmentFlag.AlignRight)
        header.addWidget(name)
        header.addWidget(value, 1)
        slider = QSlider(Qt.Orientation.Horizontal)
        slider.setAccessibleName(title)
        slider.setRange(10 if key == "Radius" else 0, maximum)
        slider.valueChanged.connect(lambda n, k=key, v=value: self._slider_changed(k, n, v))
        row.addLayout(header)
        row.addWidget(slider)
        self.controls[key] = (slider, value)
        return frame

    @staticmethod
    def _display(key, value):
        return f"{value} px" if key == "Radius" else f"{value}%"

    def _load_config(self):
        try:
            for key, default in DEFAULTS.items():
                stored = read_config(EFFECT_GROUP, key, f"{default / 100:.2f}" if key != "Radius" else default)
                numeric = float(stored)
                if not math.isfinite(numeric):
                    raise ValueError(f"Invalid {key} value: {stored}")
                self._values[key] = round(numeric if key == "Radius" else numeric * 100)
            enabled_text = read_config("Plugins", "nxglowEnabled", "false").lower()
            self._enabled = enabled_text in {"true", "1", "yes"}
            self.enabled.setChecked(self._enabled)
            for key, (slider, value_label) in self.controls.items():
                self._values[key] = min(slider.maximum(), max(slider.minimum(), self._values[key]))
                with QSignalBlocker(slider):
                    slider.setValue(self._values[key])
                value_label.setText(self._display(key, self._values[key]))
            self.status.setText("Preview mode · no settings will be changed." if DRY_RUN else "Ready · changes save automatically.")
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            self.status.setText(f"Could not read KWin settings: {exc}")
        finally:
            self._loading = False

    def _slider_changed(self, key, value, label):
        self._values[key] = value
        label.setText(self._display(key, value))
        if DRY_RUN:
            self.status.setText("Preview mode · no settings will be changed.")
            return
        self._debounce.start(150)

    def _apply_values(self):
        if DRY_RUN:
            self.status.setText("Preview mode · no settings will be changed.")
            return
        try:
            for key, value in self._values.items():
                saved = value if key == "Radius" else f"{value / 100:.2f}"
                write_config(EFFECT_GROUP, key, saved)
            subprocess.run(
                ["qdbus6", "org.kde.KWin", "/Effects",
                 "org.kde.kwin.Effects.reconfigureEffect", "nxglow"],
                check=True, capture_output=True, text=True, timeout=5,
            )
            self.status.setText("Glow settings saved.")
        except (OSError, subprocess.SubprocessError) as exc:
            self.status.setText(f"Could not apply settings: {exc}")

    def _toggle_effect(self, enabled):
        if self._loading or DRY_RUN:
            if DRY_RUN:
                self.status.setText("Preview mode · no settings will be changed.")
            return
        try:
            method = "loadEffect" if enabled else "unloadEffect"
            if method == "loadEffect":
                call_effect(method)
            else:
                subprocess.run(
                    ["qdbus6", "org.kde.KWin", "/Effects",
                     "org.kde.kwin.Effects.unloadEffect", "nxglow"],
                    check=True, capture_output=True, text=True, timeout=5,
                )
            if effect_is_loaded() != enabled:
                raise RuntimeError(f"KWin did not {'enable' if enabled else 'disable'} nx glow")
            write_config("Plugins", "nxglowEnabled", "true" if enabled else "false")
            self._enabled = enabled
            self.status.setText("Glow enabled." if enabled else "Glow disabled.")
        except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
            recovery = " Try Install / repair effect." if self.setup_button.isVisible() else ""
            self.status.setText(f"Could not change effect: {exc}.{recovery}")
            with QSignalBlocker(self.enabled):
                self.enabled.setChecked(self._enabled)

    def _reset(self):
        for key, default in DEFAULTS.items():
            slider, _ = self.controls[key]
            slider.setValue(default)
        if DRY_RUN:
            self.status.setText("Preview mode · no settings will be changed.")
        else:
            self._debounce.stop()
            self._apply_values()

    def _start_setup(self):
        if DRY_RUN:
            self.status.setText("Preview mode · installer is disabled.")
            return
        if self._setup_process.state() != QProcess.ProcessState.NotRunning:
            self.status.setText("Install / repair is already running.")
            return
        self.details.clear()
        self.status.setText("Building and installing nx glow…")
        self._setup_process.setWorkingDirectory(str(self.source_dir))
        self._setup_process.start(str(self._installer), ["--gui"])

    def _read_setup_output(self, is_error):
        data = (self._setup_process.readAllStandardError() if is_error
                else self._setup_process.readAllStandardOutput())
        output = bytes(data).decode(errors="replace").strip()
        if output:
            self.details.append(output)

    def _setup_finished(self, exit_code, exit_status):
        if exit_code == 0 and exit_status == QProcess.ExitStatus.NormalExit:
            self.status.setText("Install / repair finished.")
            self._reload_enabled()
        else:
            self.status.setText(f"Install / repair failed (exit {exit_code}). See setup details.")

    def _setup_error(self, error):
        self.status.setText(f"Could not start installer: {self._setup_process.errorString()}")

    def _reload_enabled(self):
        try:
            value = read_config("Plugins", "nxglowEnabled", "false").lower()
            self._enabled = value in {"true", "1", "yes"}
            with QSignalBlocker(self.enabled):
                self.enabled.setChecked(self._enabled)
        except (OSError, subprocess.SubprocessError) as exc:
            self.status.setText(f"Installed, but could not reload enable state: {exc}")

    def _toggle_details(self, visible):
        self.details.setVisible(visible)
        self.details_button.setText("Hide setup details" if visible else "Show setup details")

    def closeEvent(self, event):
        if self._setup_process.state() != QProcess.ProcessState.NotRunning:
            self.status.setText("Setup is still running; finish/cancel the password prompt first.")
            event.ignore()
            return
        if self._debounce.isActive():
            self._debounce.stop()
            self._apply_values()
        super().closeEvent(event)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--smoke-test", metavar="PNG", help="save a preview image and exit")
    args = parser.parse_args()
    global DRY_RUN
    if args.smoke_test:
        DRY_RUN = True
    app = QApplication(sys.argv[:1])
    window = SettingsWindow()
    window.show()
    if args.smoke_test:
        output = Path(args.smoke_test)

        def capture_and_exit():
            output.parent.mkdir(parents=True, exist_ok=True)
            if not window.grab().save(str(output)):
                print(f"Could not save preview image to {output}", file=sys.stderr)
                app.exit(1)
            else:
                print(f"NX_GLOW_SETTINGS_OK {output}", flush=True)
                app.quit()

        QTimer.singleShot(500, capture_and_exit)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
