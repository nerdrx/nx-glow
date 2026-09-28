# nx glow

Experimental native KWin effect that lets window content influence a soft desktop glow. Built against KWin 6.7.5 headers; this project is early and has not been validated for multi-monitor or HDR setups. Full-damage redraw cost has not been benchmarked.

Showcase: [nerdrx.github.io/nx-glow](https://nerdrx.github.io/nx-glow/).

![nx glow running in a real KWin compositor smoke test](docs/preview.png)

## Build and install

From this directory, run `./install.sh`. It builds `build/` locally, then uses `sudo` only to copy the plugin into Qt's system plugin directory. The script enables and loads the effect for the current user.

To build without installing, run `cmake -S . -B build && cmake --build build`. After KWin ABI updates, rebuild and reinstall the effect.

Tune the glow in `kwinrc` under `[Effect-nxglow]`: `Radius` defaults to `110` (range `10–300`); `Strength` defaults to `0.85` (range `0–2`). Edit with System Settings or `kwriteconfig6`, then apply changes with:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect nxglow
```

Remove it with `./uninstall.sh`. Neither script should be run with `sudo`; each invokes elevated commands only for system plugin files.

## Smoke test

`tests/smoke.sh` runs a disposable KWin session inside headless Gamescope, captures baseline, active, changed-content, and minimized-window frames, then checks the screenshots with `tests/check.py`. It needs Gamescope, KWin, D-Bus, Qt/PySide6, python-xlib, Pillow, and Spectacle. The screenshots and log go to `test-output/` and are local test artifacts, not project recordings.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
