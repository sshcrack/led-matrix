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
./emulator_build/install/main --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-scale=4

# Convenience wrapper (reads .env, builds, then runs)
./scripts/run_emulator.sh
```

Cross-compile for Pi and deploy:
```bash
cmake --preset cross-compile
cmake --build --preset cross-compile --target install
# build_upload.sh does the above + rsync to ledmat:/home/pi/ledmat/run/ + service restart
./build_upload.sh
```

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

The emulator binary must exist at `emulator_build/install/main` before starting the server.

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

## CI

`.github/workflows/ci.yml` builds all three presets (cross-compile, desktop-linux, desktop-windows) on push/PR to `master`. The react-web is built only for the `cross-compile` job (pnpm + node 22). Desktop jobs pass `-DSKIP_WEB_BUILD=ON`.
