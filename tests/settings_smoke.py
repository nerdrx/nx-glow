"""Exercise the actual Qt controls with an isolated fake KWin/config boundary."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
from PySide6.QtWidgets import QApplication

root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("nx_glow_settings", root / "settings.py")
settings = importlib.util.module_from_spec(spec)
spec.loader.exec_module(settings)
values = {}
loaded = False
calls = []

def run(args, **kwargs):
    global loaded
    calls.append(args)
    result = ""
    if args[0] in ("kreadconfig6", "kwriteconfig6"):
        key = (args[args.index("--group") + 1], args[args.index("--key") + 1])
        if args[0] == "kreadconfig6":
            result = values.get(key, args[args.index("--default") + 1])
        else:
            values[key] = args[-1]
    elif args[0] == "qdbus6":
        method = args[3].rsplit(".", 1)[-1]
        if method == "loadEffect":
            result, loaded = ("false" if loaded else "true"), True
        elif method == "unloadEffect":
            loaded = False  # Real KWin returns void here, not a boolean.
        elif method == "loadedEffects":
            result = "blur\nnxglow\n" if loaded else "blur\n"
    else:
        raise AssertionError(args)
    return SimpleNamespace(stdout=str(result), stderr="", returncode=0)

settings.subprocess.run = run
settings.DRY_RUN = False
app = QApplication([])
window = settings.SettingsWindow()
window.show()
app.processEvents()
window.enabled.setChecked(True)
assert loaded and values[("Plugins", "nxglowEnabled")] == "true"
window.enabled.setChecked(False)
assert not loaded and values[("Plugins", "nxglowEnabled")] == "false"
window.controls["Radius"][0].setValue(200)
window.controls["Strength"][0].setValue(50)
window.controls["Saturation"][0].setValue(150)
window._debounce.stop()
window._apply_values()
assert values[("Effect-nxglow", "Radius")] == "200"
assert values[("Effect-nxglow", "Strength")] == "0.50"
assert values[("Effect-nxglow", "Saturation")] == "1.50"
window.reset.click()
assert values[("Effect-nxglow", "Radius")] == "110"
assert values[("Effect-nxglow", "Strength")] == "0.85"
settings.DRY_RUN = True
count = len(calls)
window._apply_values()
window._start_setup()
assert len(calls) == count
app.processEvents()
out = root / "test-output/settings.png"
out.parent.mkdir(exist_ok=True)
assert window.grab().save(str(out))
window.close()
print("PASS: settings sliders, persistence, reset, enable/disable and dry-run isolation")
