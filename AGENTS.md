# AGENTS.md

## Project shape

Three binaries, one repo:
- **`src_matrix/`** — C++ daemon that runs on the Raspberry Pi, drives the LED matrix (entrypoint: `src_matrix/main.cpp`)
- **`src_desktop/`** — ImGui desktop app for GPU-intensive effects; talks to the Pi via UDP (pixel stream) and WebSocket (config) (entrypoint: `src_desktop/main.cpp`)
- **`react-web/`** — Vite + React + TypeScript PWA; talks to the Pi's REST API

Plugins live under `plugins/<Name>/` with a `matrix/` subdirectory for Pi code and an optional `desktop/` subdirectory for desktop code. Shared libraries are in `shared/common/`, `shared/matrix/`, and `shared/desktop/`.

## Build system

CMake + Ninja + vcpkg. Three presets, three distinct build dirs:

| Preset | Purpose | Build dir |
|---|---|---|
| `emulator` | x64 build with SDL2 emulator (for local development) | `emulator_build/` |
| `cross-compile` | arm64 for Raspberry Pi | `build/` |
| `desktop-linux` / `desktop-windows` | Desktop ImGui app | `desktop_build/` |

**`ENABLE_EMULATOR` and `ENABLE_DESKTOP` cannot both be ON — CMake will fatal-error.**

### Common commands

```bash
# Configure (first time, or after CMakeLists changes)
cmake --preset emulator

# Skip react-web rebuild when you haven't touched it (much faster)
cmake --preset emulator -DSKIP_WEB_BUILD=ON

# Build and install
cmake --build --preset emulator --target install

# Run the emulator (128×128, SDL window ×4)
./emulator_build/install/led-matrix --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-scale=4

# Convenience wrapper (reads .env, builds, then runs)
./scripts/run_emulator.sh
```

Desktop build on Windows (requires `VCToolsVersion` env var to align vcpkg and MSBuild toolsets):
```bash
# Configure
$env:VCToolsVersion="14.44.35207"; cmake --preset desktop-windows
# Build
$env:VCToolsVersion="14.44.35207"; cmake --build --preset desktop-windows
# Debug variant
$env:VCToolsVersion="14.44.35207"; cmake --preset desktop-windows-debug
$env:VCToolsVersion="14.44.35207"; cmake --build --preset desktop-windows-debug
```

Set `VCToolsVersion` to the latest MSVC version under `E:\PCProgs\VsCode2022\VC\Tools\MSVC\`.

Cross-compile for Pi and deploy:
```bash
cmake --preset cross-compile
cmake --build --preset cross-compile --target install
# build_upload.sh does the above + rsync to ledmat:/home/pi/ledmat/run/ + service restart
./build_upload.sh
```

### DEB packaging (cross-compile)

The cross-compile preset now produces both `.tar.gz` and `.deb` packages. The `.deb` installs to FHS paths (`/usr/bin/`, `/usr/lib/led-matrix/`, `/usr/share/led-matrix/`) via CPack.

Package the DEB:
```bash
cmake --preset cross-compile
cmake --build --preset cross-compile --target package
# Produces build/led-matrix-<version>-arm64.deb
```

The DEB includes:
- **debconf templates** — interactive prompts for hardware config (rows, cols, chain, parallel), Spotify credentials, and auto-update settings on first install
- **postinst** — writes `/etc/default/led-matrix` (flags + Spotify env), creates `/var/lib/led-matrix/` owned by `pi:pi`, migrates data from `/opt/led-matrix/` if present, enables and starts the systemd service
- **prerm/postrm** — clean stop/disable on remove, purge `/var/lib/led-matrix` and debconf answers on `dpkg --purge`
- **systemd unit** (`/lib/systemd/system/led-matrix.service`) — runs as `root` (required for GPIO `/dev/mem` access), reads `EnvironmentFile=/etc/default/led-matrix`, the library drops privileges to `pi:pi` after init via `--led-drop-priv-user=pi --led-drop-priv-group=pi`

### Service user model

- The systemd service runs as **root** (`User=root`) because `rpi-rgb-led-matrix` needs to open `/dev/mem` for GPIO
- After matrix initialisation, the library automatically drops privileges via `setresuid`/`setresgid` to `pi:pi` (controlled by `--led-drop-priv-user=pi --led-drop-priv-group=pi` in `MATRIX_OPTS`)
- Runtime data lives in `/var/lib/led-matrix/` (owned by `pi:pi`) so the process can write config files after privilege drop
- To reconfigure: `sudo dpkg-reconfigure led-matrix` — this re-runs the debconf `config` script and `postinst`, updating `/etc/default/led-matrix`

### FHS layout (DEB)

| Content | Path |
|---|---|
| Main binary | `/usr/bin/led-matrix` |
| Shared libs | `/usr/lib/led-matrix/*.so` |
| Plugins | `/usr/lib/led-matrix/plugins/` |
| Update script | `/usr/lib/led-matrix/update_service.sh` |
| Web UI | `/usr/share/led-matrix/web/` |
| Scene previews | `/usr/share/led-matrix/scene_previews/` |
| Fonts | `/usr/share/led-matrix/7x13.bdf` |
| Licenses | `/usr/share/doc/led-matrix/licenses/` |
| Copyright | `/usr/share/doc/led-matrix/copyright` |
| config.json | `/var/lib/led-matrix/config.json` |
| Custom user data | `/var/lib/led-matrix/data/` |
| Update flags | `/var/lib/led-matrix/.update_success` / `.update_error` |
| Env config | `/etc/default/led-matrix` |

### clangd

`.clangd` points at `emulator_build/` for `compile_commands.json`. Configure the emulator preset before expecting IDE completion to work.

### Web frontend

```bash
cd react-web
pnpm install   # installs deps (scripts/install_deps.sh wraps this)
pnpm run build # tsc + vite build → dist/ (gets bundled into the matrix binary install)
pnpm run dev   # local dev server
```

The matrix CMake build embeds `react-web/dist/` into the installed binary tree. Add `-DSKIP_WEB_BUILD=ON` on every `cmake --preset` reconfigure when you haven't changed the web app.

## MCP emulator server

An MCP server in `scripts/mcp-emulator/server.py` lets AI agents control the emulator directly:

```bash
# One-time setup
./scripts/mcp-emulator/setup.sh

# Server is configured via .mcp.json (already present in the repo root)
```

Tools exposed: `start_emulator`, `stop_emulator`, `get_status`, `get_frame` (returns PNG of the current matrix), `list_scenes`, `list_presets`, `set_preset`, `http_api`.

The emulator binary must exist at `emulator_build/install/led-matrix` before starting the server.

## Plugin development

Plugins auto-register via `register_plugin()` in their `CMakeLists.txt`. The macro is defined in `plugins/CMakeLists.txt`. No changes to the root `CMakeLists.txt` or `vcpkg.json` are needed for plugins with no extra vcpkg dependencies.

Minimal plugin structure:
```
plugins/MyPlugin/
  CMakeLists.txt          ← register_plugin(MyPlugin matrix/... DESKTOP desktop/...)
  matrix/
    MyPlugin.cpp          ← extern "C" createMyPlugin / destroyMyPlugin
    MyPlugin.h
    scenes/MyScene.cpp
    scenes/MyScene.h
```

`extern "C"` factory names **must match the plugin name exactly** — `createMyPlugin` / `destroyMyPlugin`. The `PLUGIN_NAME` macro is injected by CMake via `target_compile_definitions`.

Key headers:
- Scene base class: `shared/matrix/include/shared/matrix/Scene.h`
- Plugin entry: `shared/matrix/include/shared/matrix/plugin/main.h`
- Properties (auto-serialized to JSON config): `shared/matrix/include/shared/matrix/plugin/property.h` — use the `MAKE_PROPERTY` macro
- Desktop plugin entry: `shared/desktop/include/shared/desktop/plugin/main.h`

For plugins with additional vcpkg dependencies, add a feature entry to `vcpkg.json` named `<kebab-plugin-name>-matrix` or `<kebab-plugin-name>-desktop`.

## Design constraints (read before proposing features)

See `FEATURES.md` for the authoritative list. Summary:
- **Low compute on the Pi** — scenes run on a single RPi 4, no GPU
- **Non-interactive** — the matrix is a passive ambient prop; no input from viewers
- **No readable text** — no tickers, scores, headlines, or anything requiring active reading
- **Visually interesting** — ambient/aesthetic purpose only

Ideas already explicitly rejected: Home Assistant/MQTT, WebSocket live preview, RSS/news/sports/crypto tickers, scrolling text, web paint canvas, emoji signals. Check `FEATURES.md` before implementing anything in these areas.

## Environment quirks

- Cross-compile toolchain is downloaded automatically by `cmake/PI.cmake` on first configure if `CROSS_COMPILE_ROOT` env var isn't set and `build/cross-compile/` doesn't exist.
- vcpkg binary cache for CI uses a private NuGet feed (`nuget.pkg.github.com/sshcrack`) requiring `GH_PACKAGES_TOKEN`. Locally, vcpkg builds from source.
- A `.env` file in the repo root is sourced by `scripts/run_emulator.sh` if present (useful for `SPOTIFY_CLIENT_ID` etc.).
- The devcontainer (`postCreateCommand`) runs `git submodule update --init --recursive` — submodules are required.
- WSL users: see `scripts/WSL_SYNC_README.md` for the file-watch sync setup between Windows and WSL.

## Preview Generator (`src_preview_gen`)

The `src_preview_gen` binary generates animated GIF previews for all registered matrix scenes. It is **tightly coupled to the emulator preset** — it requires `ENABLE_EMULATOR` at compile time and instantiates `EmulatorMatrix` directly. It will **not** build under the cross-compile or desktop presets.

Usage after building the emulator preset:
```bash
# Generate previews for all scenes (requires emulator preset build)
./emulator_build/install/preview_gen --output ./previews

# Dump scene manifest as JSON
./emulator_build/install/preview_gen --dump-manifest

# Generate specific scenes
./emulator_build/install/preview_gen --scenes SceneA,SceneB --frames 60 --fps 10
```

Scenes that require the desktop app (`needs_desktop` = true) are skipped automatically. Use `scripts/capture_desktop_preview.sh` to capture them manually.

## CI

`.github/workflows/ci.yml` builds all three presets (cross-compile, desktop-linux, desktop-windows) on push/PR to `master`. The react-web is built only for the `cross-compile` job (pnpm + node 22). Desktop jobs pass `-DSKIP_WEB_BUILD=ON`.
