"""Controlled XWayland scene for the isolated KWin smoke test."""
import os
import subprocess
import sys
import traceback
from pathlib import Path
from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import QApplication, QWidget, QLabel, QVBoxLayout
from Xlib import Xatom, display

app = QApplication([])
def fail(kind, value, tb):
    traceback.print_exception(kind, value, tb)
    app.exit(1)
sys.excepthook = fail
out = Path(os.environ["NX_GLOW_TEST_OUTPUT"])
desktop = QWidget(None, Qt.WindowType.Desktop | Qt.WindowType.FramelessWindowHint)
desktop.setStyleSheet("background:#080b12;")
desktop.setGeometry(0, 0, 1440, 900)
xdisplay = display.Display()
xwindow = xdisplay.create_resource_object("window", int(desktop.winId()))
xwindow.change_property(xdisplay.intern_atom("_NET_WM_WINDOW_TYPE"),
                        Xatom.ATOM, 32, [xdisplay.intern_atom("_NET_WM_WINDOW_TYPE_DESKTOP")])
xdisplay.sync()
desktop.show()
windows = []
for x, y, colour, name in [(240, 200, "#146aff", "Ocean blue"), (800, 380, "#ff481e", "Ember orange")]:
    w = QWidget()
    w.setWindowTitle(name)
    w.setGeometry(x, y, 380, 260)
    w.setStyleSheet(f"background:{colour};color:white;font-size:24px;")
    layout = QVBoxLayout(w)
    layout.addWidget(QLabel(name))
    w.show()
    windows.append(w)

def dbus(method, *args):
    return subprocess.check_output(["qdbus6", "org.kde.KWin", "/Effects", "org.kde.kwin.Effects." + method, *args], text=True).strip()

def capture(name):
    subprocess.run(["spectacle", "-b", "-n", "-f", "-o", str(out / name)],
                   env={**os.environ, "QT_QPA_PLATFORM": "wayland"}, check=True, timeout=12)

def baseline():
    dbus("unloadEffect", "nxglow")
    QTimer.singleShot(700, first)

def first():
    capture("before.png")
    assert dbus("loadEffect", "nxglow") == "true", "effect failed to load"
    QTimer.singleShot(1200, second)

def second():
    capture("after.png")
    windows[0].setStyleSheet("background:#20ed60;color:white;font-size:24px;")
    QTimer.singleShot(1000, third)

def third():
    capture("changed.png")
    windows[0].showMinimized()
    QTimer.singleShot(1000, fourth)

def fourth():
    capture("minimized.png")
    windows[0].setStyleSheet("background:#146aff;color:white;font-size:24px;")
    windows[0].showNormal()
    for key, value in (("Radius", "200"), ("Strength", "1"), ("Saturation", "0")):
        subprocess.run(["kwriteconfig6", "--file", "kwinrc", "--group", "Effect-nxglow", "--key", key, value], check=True)
    dbus("reconfigureEffect", "nxglow")
    QTimer.singleShot(1000, fifth)

def fifth():
    capture("tuned.png")
    subprocess.run(["kwriteconfig6", "--file", "kwinrc", "--group", "Effect-nxglow", "--key", "Strength", "0"], check=True)
    dbus("reconfigureEffect", "nxglow")
    QTimer.singleShot(1000, sixth)

def sixth():
    capture("zero.png")
    for key, value in (("Radius", "300"), ("Strength", "2"), ("Saturation", "1")):
        subprocess.run(["kwriteconfig6", "--file", "kwinrc", "--group", "Effect-nxglow", "--key", key, value], check=True)
    dbus("reconfigureEffect", "nxglow")
    QTimer.singleShot(1000, seventh)

def seventh():
    capture("quality.png")
    dbus("unloadEffect", "nxglow")
    print("NX_GLOW_SCENE_OK", flush=True)
    app.quit()

QTimer.singleShot(1500, baseline)
sys.exit(app.exec())
