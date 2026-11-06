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

### Worktrees and build caching

Fresh worktrees build in seconds instead of minutes: `cmake/BuildCache.cmake` finds vcpkg when `VCPKG_ROOT` is unset and routes compiles through ccache with `base_dir=<worktree>`, so object files are shared across all worktrees and the main checkout; vcpkg ports come from its binary cache. Run `scripts/setup_worktree.sh [--build]` in a new worktree (the T3 project runs it on worktree creation): it copies the git-ignored `.env`, `react-web/dist` and `scene_previews/` from the main checkout and configures the emulator preset. Build only through the presets; a hand-rolled `cmake -B` without the presets misses the shared cache.

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

### Sanitizers, warnings and core dumps

`-DLED_MATRIX_SANITIZE=address` (ASan+UBSan, UB aborts) or `=thread` (TSan) instruments every in-tree target, plugin DSOs included (`cmake/Sanitizers.cmake`; ASan and TSan are mutually exclusive). Presets with their own build dirs and `SKIP_WEB_BUILD=ON`: `emulator-asan` (`emulator_asan_build/`), `emulator-tsan`, `desktop-linux-asan` (`desktop_asan_build/`), `desktop-linux-tsan`.

```bash
cmake --preset emulator-asan && cmake --build --preset emulator-asan --target install
source scripts/sanitizers/env.sh   # *SAN_OPTIONS + third-party suppressions
./emulator_asan_build/install/bin/led-matrix --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-headless
ctest --test-dir emulator_asan_build --output-on-failure
```

- `scripts/sanitizers/{lsan,tsan,ubsan,asan}.supp` suppress **third-party frames only** (SDL, GLFW, Mesa, portaudio, GraphicsMagick, ...). Fix our own findings instead of suppressing them.
- Sanitizer builds define `LED_MATRIX_SANITIZER_BUILD` and do not `dlclose()` plugins, so exit-time reports still symbolize plugin frames.
- Every build defines `_GLIBCXX_ASSERTIONS` (bounds-checked `operator[]` etc.; aborts with a message instead of corrupting memory).
- Our targets (not `thirdparty/`, not vcpkg) build with `-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wnull-dereference -Wuse-after-free=3` (`cmake/Warnings.cmake`). Warnings only, no `-Werror`; don't add new ones.
- Core dumps: the crash reporter writes `crashes/crash-*.txt` and then re-raises the signal, so the kernel still dumps core. The systemd unit sets `LimitCORE=infinity`; on the Pi install `systemd-coredump` (it also sets `fs.suid_dumpable=2`, which the daemon needs because it drops privileges after GPIO init). Then `coredumpctl list led-matrix`, `coredumpctl info led-matrix` (symbolized backtrace) and `coredumpctl debug led-matrix` (gdb; the `RelWithDebInfo` binary has symbols). Locally, run `ulimit -c unlimited` before reproducing.
- Findings and open risks so far: `docs/memory-safety-report.md`.

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

### Web UI architecture and plugin UI extensions

`react-web/src/core/` is the shared layer (data, actions, editing engine, reusable UI) and `react-web/src/shell/` is the "Console" layout: a top bar with a ⌘K command palette, and a one-screen home with preset rail · live preview · inline inspector (`?edit=<id>` edits a preset, `?new=1` opens the quick builder; `/modify-preset/:id` redirects there). Screens never call the API themselves: `useMatrix()` (`core/matrixContext.ts`) exposes status/presets/scenes plus optimistic actions (`setPower`, `setMode`, `activate`, `createPreset`, `savePreset`, …), `usePresetEditor` autosaves edits (no Save button, delete-with-undo), and `QuickBuilder` creates *and plays* a preset from picked scenes / ambiance packs / plugin templates in one step. On phones (<1024px) the shell switches to a bottom tab bar + always-visible mini-player, and the inspector (editor/builder) opens as a full-height bottom sheet (`core/ui/Sheet.tsx`, `useMediaQuery`) instead of inline; keep touch targets ≥40px and inputs ≥16px there. Design tokens live in `index.css` (dark by default, `core/colorScheme.ts`).

A plugin can extend the web UI by adding `plugins/<Name>/web/index.ts(x)` with `export default definePluginUi({...})` (`core/pluginUi.ts`). `core/pluginRegistry.ts` discovers these with `import.meta.glob` (one lazy chunk per plugin) — no registration, no CMake or other UI change; plugins without `web/` keep working from the metadata `/list_scenes` already sends. Extension points: `propertyControls` (custom control keyed by the `control` id a property declares via `.control("…")` in C++; real example: `plugins/Video/web/` provides `video_playlist`, the YouTube search + playlist editor), `sceneActions` (extra buttons on a scene; the "Configure sources" button is itself one, in `pluginRegistry.ts`), `presetTemplates` (one-tap starting points in the quick builder; example `plugins/AmbientScenes/web/index.ts`). 

**Runtime loading.** `pnpm build` also runs `scripts/build-plugin-uis.mjs`, which turns every `plugins/<Name>/web/index.ts(x)` into a standalone bundle `dist/plugin-ui/<Name>/{ui.js,ui.css,ui.json}` (installed with the web app to `share/led-matrix/web/plugin-ui/`). `react`, `react/jsx-runtime` and `~/plugin-sdk` are externals bound to `globalThis.__LED_MATRIX_SDK__` (`core/pluginSdk.ts`); lucide icons are bundled into the plugin; `ui.css` is Tailwind generated from the plugin's own sources with the host theme tokens. **Plugin web modules may import only `react`, `lucide-react` and `~/plugin-sdk`** (`src/plugin-sdk.ts` is the whole host API: keep it a list of `export { … } from` lines, the build script reads the names from it). Adding a name there is additive; removing one needs a `UI_API_VERSION` bump (in `pluginRegistry.ts` and the build script).

The daemon's `GET /plugin_ui` returns `{ui_api_version, plugins:[installed names], web_ui:{<Name>:{api_version, js, css?}}}` and `GET /plugin_ui/<Name>/<ui.js|ui.css>` serves the files (only those two names, only for loaded plugins). The daemon looks in `<plugin dir>/web/` (next to `lib<Name>.so`, so a plugin can ship or update its UI by itself) and then in `share/led-matrix/web/plugin-ui/<Name>/`. `MatrixProvider` calls `loadPluginUis(apiUrl)`: a plugin with a runtime bundle is loaded from it (fetched through the authenticated API and imported from a Blob URL, so tokens and the dev proxy work), otherwise the module compiled into the build is used; if the daemon speaks a newer `ui_api_version` than the build, or the bundle's `api_version` is too new, it is skipped; an older daemon without the route gets all bundled modules. Limits: a runtime bundle can only use the SDK names above plus its own code, and its CSS only covers the Tailwind classes it uses itself.

Dev against a real daemon without CORS trouble: `LED_MATRIX_DEV_API=http://localhost:8080 pnpm dev`, then set the API URL to `http://localhost:5173/__api` (proxies REST + `/live_frame` WebSocket).

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

Music director note (2026-11-05): while live music plays, both directors prefer scenes tagged `music-showcase` (longer dwell) and match `style:<chill|edm|party|rock|hiphop|acoustic>` tags against the current music style (`shared/matrix/music_style.h`). The style fuses genre metadata (`RuntimeInputIds::MusicGenre`, published by SpotifyScenes `genre_lookup.cpp`: Spotify artist genres first, then Deezer, then the optional GetSongBPM key; cached in `cache/genres.json`) with a weak audio estimate. `director_music_sim <song.audr> [--desktop --spotify --genre "a,b"]` replays recorded analysis (from `tests/audio_analysis_dump.cpp`) through the real director to check behaviour. Every `music-showcase` scene needs a `style:` tag; `tests/music_showcase_coverage.py` enforces a minimum scene count per style (higher for hiphop/rock/party, the owner's main genres).

SpotifyMV note: standalone plugin that plays YouTube music videos on Spotify track change; audio is intentionally disabled, yt-dlp/ffmpeg decoding happens on the desktop and the Pi only copies UDP pixels. `VideoStreamEngine` lives in `SharedToolsDesktop`; cache is keyed by Spotify track_id.

Pi vs desktop quality: `Scene::render_quality_scale()` (0.5..1) is the one quality knob. It is 1.0 on the desktop worker and in previews; on the Pi it adapts to measured render cost (`set_render_quality_hint()` gives a Pi-only start value). Scenes with optional expensive work map it to cheaper work: particle counts, or smooth layers drawn through `AudioKit::Buffer::add_field()` with `field_step()` (per pixel at full quality, an interpolated lattice below). The Pi offloads a scene to the desktop scene worker when its p95 exceeds 72% of the frame budget or its quality is below 1, and the worker renders full quality. Check the reduced look with `preview_gen --quality 0.6` / `render_scene_video.sh --quality 0.6`. Cache static per-pixel geometry instead of recomputing it each frame, and prefer `AudioKit::len` over `std::hypot` (a libm call on the Pi).

Desktop offload compatibility: the worker runs the desktop app's own installed plugins, so it advertises a scene-code fingerprint (`Common::scene_code_digest()`, a git tree hash of `plugins/` + `shared/` from `cmake/SceneCodeDigest.cmake`); the Pi only offloads to a worker whose fingerprint matches its own. `/diagnostics` → `remote_render` shows both fingerprints, `render_placement.reason` says why a scene rendered where it did.

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

## Scene videos and PR media

Show visual changes in PRs as videos, not descriptions:
- `scripts/render_scene_video.sh --scenes <a,b> [--replay song.audr] [--audio song.wav]` renders MP4s with an LED-panel look via `preview_gen` (emulator preset).
- Real-music input: `emulator_build/install/bin/audio_analysis_dump <song.wav> <out.audr>` records the desktop analyzer's output; pass it as `--replay`. Keep copyrighted audio out of anything uploaded: publish silent videos and keep `--audio` versions local.
- `spotify` previews take custom artwork and track changes: `--preview-option 'spotify.playback:covers=["a.png","b.png"]' --preview-option spotify.playback:track_seconds=6`.
- `scripts/pr_media.sh [--comment] <files>` uploads images/MP4s to the rolling `pr-media` prerelease and prints markdown (videos get a linked GIF preview, since GitHub only plays inline video uploaded through the browser). Paste the output into the PR body or use `--comment`.

## Keeping the repo small

Every committed blob stays in history forever and slows every `git clone`. History was rewritten on 2026-11-04 to drop ~140 MiB of old GIFs, a 51 MiB `.aab` and profiler dumps (clone ~188 MiB → ~46 MiB). Keep it that way:

- `scripts/check_repo_size.sh` fails on files > 2 MiB, blocked artifact types (`.aab .apk .zip .deb heaptrack.*` …) and > 10 MiB of added/modified files per PR. CI runs it on every PR (`repo-size` job); enable the local hook once per clone with `git config core.hooksPath .githooks`.
- Exempt a deliberate file via `.repo-size-allow`; waive the total budget with `[large-ok]` in a commit message.
- **`scene_previews/*.gif` are not tracked in git** (each regeneration cost 0.5–1.5 MiB of history forever; the guard blocks them). They live as assets of the rolling `scene-previews` **prerelease**: `./scripts/generate_scene_previews.sh …` then `./scripts/publish_scene_previews.sh` (needs `gh auth login`; uploads only changed files, removes stale ones). `./scripts/fetch_scene_previews.sh [--if-missing]` downloads them into `scene_previews/` (CMake installs that dir; CI fetches before the cross-compile/.deb build; `run_emulator.sh` and `build_upload.sh` fetch when missing). The release publishes `scene_previews.tar.gz` (exact names, what builds use) plus individual URL-safe GIFs for README embeds. It is a prerelease with a non-`v*` tag so `/releases/latest`, the updater and `create-release.yml` ignore it; `/api/update/releases` filters it out. Never delete it.
- Build outputs, profiles, videos belong in release assets, not git.

## CI

`.github/workflows/ci.yml` on push/PR to `master` builds only the react-web frontend (pnpm + node 22) and uploads it as the `web-build` artifact. It then delegates all three C++ builds (`cross-compile`, `desktop-linux`, `desktop-windows`) to the reusable workflow `.github/workflows/build-matrix.yml`, which downloads the `web-build` artifact so all presets get the bundled web UI without rebuilding it.
