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
./emulator_build/install/bin/led-matrix --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-scale=4

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
# Produces build/led-matrix-<version>_arm64.deb
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

The emulator binary must exist at `emulator_build/install/bin/led-matrix` before starting the server.

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

The matrix is a passive prop in a room. Every scene must be:
- **Low compute on the Pi** — scenes run on a single RPi 4, no GPU
- **Non-interactive** — no input from viewers (configuration via the web UI is fine; the running display needs no input)
- **No readable text** — no tickers, scores, headlines, or anything requiring active reading
- **Visually interesting** — ambient/aesthetic purpose only

Rejected ideas (do not implement without asking the owner):

| Idea | Reason |
|---|---|
| Home Assistant / MQTT | Not needed |
| Continuously pushed / always-on live preview | Too heavy on the Pi; live capture must stay viewer-demand-driven (pull-based, see "Live web matrix capture performance") |
| RSS / news, sports scores, crypto/stock tickers, scrolling text | Violates the no-text rule |
| Web paint / pixel canvas | Too interactive |
| Emoji signal scene | Implies active triggering from a user |

Scenery guidance (2026-09-06): new scenery should prioritize calm, recognizable environments that reward occasional glances (aquariums, planetary vistas, natural landscapes with sparse events). Standalone crystal/gyroid sculpture shaders were rejected and replaced.

Live preview rule (2026-08-22): `/live_frame` is a WebSocket-only, pull-based transport (no HTTP polling). A connected socket does no capture work; each browser `next` message requests exactly one future composed frame, concurrent viewers coalesce, and hidden/off-screen previews stop requesting.

Music director note (2026-11-05): while live music plays, both directors prefer scenes tagged `music-showcase` (longer dwell) and match `style:<chill|edm|party|rock|hiphop|acoustic>` tags against the current music style (`shared/matrix/music_style.h`). The style fuses genre metadata (`RuntimeInputIds::MusicGenre`, published by SpotifyScenes `genre_lookup.cpp`: Spotify artist genres first, then Deezer, then the optional GetSongBPM key; cached in `cache/genres.json`) with a weak audio estimate. `director_music_sim <song.audr> [--desktop --spotify --genre "a,b"]` replays recorded analysis (from `tests/audio_analysis_dump.cpp`) through the real director to check behaviour.

SpotifyMV note: standalone plugin that plays YouTube music videos on Spotify track change; audio is intentionally disabled, yt-dlp/ffmpeg decoding happens on the desktop and the Pi only copies UDP pixels. `VideoStreamEngine` lives in `SharedToolsDesktop`; cache is keyed by Spotify track_id.

## API security

`src_matrix/server/request_guard.{h,cpp}` wraps the REST router (unit-tested by `tests/request_guard_smoke.cpp`):
- **Cross-origin browser requests are rejected** (`Origin` vs `Host`, and `Sec-Fetch-Site: cross-site` when no `Origin` is sent). `ENABLE_CORS` builds disable this check.
- **Optional token**: set `LED_MATRIX_API_TOKEN` (debconf `led-matrix/api-token` writes it to `/etc/default/led-matrix`). When set, requests must carry it as `Authorization: Bearer`, `X-Api-Token`, the `led_matrix_token` SameSite=Strict cookie (set by the web UI's token prompt), or `?token=`. `/`, `/web` and `/web/*` are always public so the UI can load and ask for the token.
- `LED_MATRIX_ALLOWED_ORIGINS` (comma-separated) allowlists extra browser origins, e.g. the Vite dev server `http://localhost:5173`.
- The desktop app has an "API token" setting; it sends it on its WebSocket/HTTP calls and exports it as `LED_MATRIX_API_TOKEN` to scene workers.

## Environment quirks

- Cross-compile toolchain is downloaded automatically by `cmake/PI.cmake` on first configure if `CROSS_COMPILE_ROOT` env var isn't set and `build/cross-compile/` doesn't exist.
- vcpkg binary cache for CI reads from `https://nuget.sshcrack.me/v3/index.json`. Locally, vcpkg builds from source.
- A `.env` file in the repo root is sourced by `scripts/run_emulator.sh` if present (useful for `SPOTIFY_CLIENT_ID` etc.).
- The devcontainer (`postCreateCommand`) runs `git submodule update --init --recursive` — submodules are required.
- WSL users: see `scripts/WSL_SYNC_README.md` for the file-watch sync setup between Windows and WSL.

## Preview Generator (`src_preview_gen`)

The `src_preview_gen` binary generates animated GIF previews for all registered matrix scenes. It is **tightly coupled to the emulator preset** — it requires `ENABLE_EMULATOR` at compile time and instantiates `EmulatorMatrix` directly. It will **not** build under the cross-compile or desktop presets.

Usage after building the emulator preset:
```bash
# Generate previews for all scenes (requires emulator preset build)
./emulator_build/install/bin/preview_gen --output ./previews

# Dump scene manifest as JSON
./emulator_build/install/bin/preview_gen --dump-manifest

# Generate specific scenes
./emulator_build/install/bin/preview_gen --scenes SceneA,SceneB --frames 60 --fps 10
```

Preview eligibility and dependencies are declared by `Scene::get_preview_spec()`. Non-desktop scenes are previewable by default; desktop-dependent scenes default to disabled until they explicitly opt in. A scene can request named fixture inputs and preview-only property overrides:

```cpp
[[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
    auto spec = Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    spec.set_property("audio_reactive", true);
    return spec;
}
```

Plugins own fixture implementations by overriding `BasicPlugin::create_preview_data_providers()` and returning `Previews::DataProvider` adapters. Providers should update the same shared/plugin state the production source updates; `preview_gen` is deliberately unaware of the provider's payload. It discovers providers, resolves the scene's input IDs, calls `begin()`, `update(SceneFrameContext)` for every preview frame, then `end()`. Registry validation rejects duplicate/missing provider IDs and invalid preview property overrides.

Current providers include `audio` (owned by AudioVisualizer) and `spotify.playback` (owned by SpotifyScenes). The Spotify provider supplies deterministic playback + local synthetic artwork, so `spotify` previews need no credentials/network. Generic fixture tuning uses repeatable `--preview-option <provider>:<key>=<json>`, e.g. `--preview-option audio:bpm=132 --preview-option spotify.playback:duration_ms=30000`. `--audio-bpm` / `--audio-profile` remain compatibility aliases. Only scenes whose preview spec remains disabled require `scripts/capture_desktop_preview.sh`.

### Live web matrix capture performance

Live matrix capture is strict pull/on-demand. The React UI normally keeps one `/live_frame` WebSocket open and sends a `next` message only when it wants one future composed frame; simultaneous viewers are coalesced into one capture and all viewers whose request preceded that capture receive the same binary `LMF1` frame. When there is no outstanding demand, `LiveFrameSnapshot` must not read matrix pixels, allocate RGB buffers, take the publish mutex, or run a lease/timer. A connected WebSocket by itself is **not** demand. The React preview stops requesting frames while hidden/off-screen.

## Keeping the repo small

Every committed blob stays in history forever and slows every `git clone`. History was rewritten on 2026-11-04 to drop ~140 MiB of old GIFs, a 51 MiB `.aab` and profiler dumps (clone ~188 MiB → ~46 MiB). Keep it that way:

- `scripts/check_repo_size.sh` fails on files > 2 MiB, blocked artifact types (`.aab .apk .zip .deb heaptrack.*` …) and > 10 MiB of added/modified files per PR. CI runs it on every PR (`repo-size` job); enable the local hook once per clone with `git config core.hooksPath .githooks`.
- Exempt a deliberate file via `.repo-size-allow`; waive the total budget with `[large-ok]` in a commit message.
- **`scene_previews/*.gif` are not tracked in git** (each regeneration cost 0.5–1.5 MiB of history forever; the guard blocks them). They live as assets of the rolling `scene-previews` **prerelease**: `./scripts/generate_scene_previews.sh …` then `./scripts/publish_scene_previews.sh` (needs `gh auth login`; uploads only changed files, removes stale ones). `./scripts/fetch_scene_previews.sh [--if-missing]` downloads them into `scene_previews/` (CMake installs that dir; CI fetches before the cross-compile/.deb build; `run_emulator.sh` and `build_upload.sh` fetch when missing). The release publishes `scene_previews.tar.gz` (exact names, what builds use) plus individual URL-safe GIFs for README embeds. It is a prerelease with a non-`v*` tag so `/releases/latest`, the updater and `create-release.yml` ignore it; `/api/update/releases` filters it out. Never delete it.
- Build outputs, profiles, videos belong in release assets, not git.

## CI

`.github/workflows/ci.yml` on push/PR to `master` builds only the react-web frontend (pnpm + node 22) and uploads it as the `web-build` artifact. It then delegates all three C++ builds (`cross-compile`, `desktop-linux`, `desktop-windows`) to the reusable workflow `.github/workflows/build-matrix.yml`, which downloads the `web-build` artifact so all presets get the bundled web UI without rebuilding it.
