---
description: Analyze the LED matrix controller and suggest grounded features
agent: plan
---

# Feature Exploration — led-matrix

You are a feature designer for a C++ LED matrix controller that turns RGB LED
panels into ambient visual canvases — runs on a Raspberry Pi with a desktop
companion app for GPU-intensive effects and a web PWA for remote control.

## Step 1 — Catalog existing features

Scan `src_matrix/`, `shared/matrix/`, `shared/common/`, `shared/desktop/`, and
`plugins/` to build a precise inventory of what already exists. Read at least
the key files below so you can distinguish "done" from "not done". Pay special
attention to stubs, TODOs, and commented-out code.

**Must-read files** (read their full contents):

- `shared/matrix/include/shared/matrix/Scene.h` — Scene base class; every scene
  derives from this. Defines lifecycle (`initialize`, `render`, `after_render_stop`,
  `before_transition_stop`), the property system integration, weighting/duration,
  and `needs_desktop_app()`.
- `shared/matrix/include/shared/matrix/plugin/main.h` — `BasicPlugin` base class.
  Every plugin provides `create_scenes()`, and optionally `create_effects()`,
  `create_transitions()`, `register_routes()`, `on_websocket_message()`,
  `on_udp_packet()`, and lifecycle hooks (`before_server_init`,
  `after_server_init`, `pre_exit`). Point of entry for extending the matrix daemon.
- `shared/matrix/include/shared/matrix/plugin_loader/loader.h` — `PluginManager`
  singleton; loads `.so` shared libraries, holds all `SceneWrapper`s,
  `ImageProviderWrapper`s, `ShaderProviderWrapper`s. The registry new plugin
  types register into.
- `shared/matrix/include/shared/matrix/config/MainConfig.h` — Persistent JSON
  config (`config.json`); manages presets, schedules, Spotify auth, update
  settings, plugin configs. Hot-reload capable via `mark_dirty()`.
- `shared/matrix/include/shared/matrix/config/data.h` — Core data structures:
  `Preset`, `Schedule`, `SpotifyData`, `UpdateSettings`, `Root`. JSON
  serialization hooks live here.
- `shared/matrix/include/shared/matrix/post_processor.h` — `PostProcessor`;
  applies registered `PostProcessingEffect`s (flash, rotate, etc.) to every frame.
  Effects are registered by plugins and triggered via REST API.
- `shared/matrix/include/shared/matrix/transition_manager.h` — `TransitionManager`;
  holds registered `TransitionEffect`s for scene-to-scene transitions (blend,
  swipe, morph, dissolve, etc.).
- `shared/matrix/include/shared/matrix/plugin/property.h` — Auto-serialized
  property system (`MAKE_PROPERTY` macro). Supports `int`, `float`, `double`,
  `bool`, `string`, `Color`, `tmillis_t`, `json`, and `EnumProperty`. Used by
  all scenes and plugins for configurable parameters exposed via REST API.
- `shared/matrix/include/shared/matrix/wrappers.h` — `SceneWrapper`,
  `ImageProviderWrapper`, `ShaderProviderWrapper` — lazy-create wrappers plugins
  use to vend scene/provider instances.
- `shared/matrix/include/shared/matrix/server/common.h` — Server globals:
  WebSocket registry, desktop connection tracking, current scene pointer, REST
  router type alias.
- `src_matrix/main.cpp` — Application entrypoint; initialization order: Magick,
  CLI args, matrix creation, post-processor + transition manager init, plugin
  loading, config loading, UpdateManager start, plugin lifecycle hooks, REST
  server thread, UDP server, hardware mainloop. Understand the full boot
  sequence here.
- `src_matrix/matrix_control/canvas.cpp` — Scene rendering & selection loop.
  Weighted random scene selection, per-scene duration, transition scheduling,
  cross-fade blending with adaptive render-rate throttling. This is where the
  display loop lives.
- `src_matrix/matrix_control/hardware.cpp` — Hardware mainloop; checks scheduling,
  handles on/off state, dispatches to `update_canvas()`. Interrupt handling.
- `src_matrix/server/server.cpp` — REST API route registration. Composes all
  route modules and also calls `plugin->register_routes()` for each loaded plugin.
  Shows the full endpoint surface area.
- `src_matrix/server/desktop_ws.h` (+ `desktop_ws.cpp`) — WebSocket server for
  desktop app communication (active scene notifications, plugin messages).
- `src_matrix/server/schedule_management.h` (+ `schedule_management.cpp`) —
  Schedule CRUD routes (`GET/POST/DELETE /schedule`, `GET /schedules`,
  `GET /scheduling_status`, `GET /set_scheduling_enabled`).
- `shared/matrix/include/shared/matrix/update/UpdateManager.h` — Auto-update
  system: checks GitHub releases, downloads tarballs, installs with service
  restart. REST API routes in `src_matrix/server/update_routes.cpp`.

There are **30+ scenes** across **12 scene-providing plugins**, **2 post-processing
effects**, and **8 transition effects**. Skim at least one plugin factory file to
understand the pattern:

- `plugins/AmbientScenes/matrix/AmbientScenes.cpp` — Largest plugin (9 scenes);
  typical factory pattern, no desktop component.
- `plugins/BasicEffects/matrix/BasicEffects.cpp` — Post-processing-only plugin
  (flash + rotate effects).
- `plugins/Transitions/matrix/Transitions.cpp` — Transition-only plugin (8
  transition effects including blend, swipe, morph, dissolves).
- `plugins/SpotifyScenes/matrix/SpotifyScenes.cpp` — External API integration
  with OAuth flow; custom REST routes.
- `plugins/Video/matrix/VideoPlugin.cpp` — Desktop-required plugin streaming
  video frames via UDP.
- `plugins/PixelJoint/matrix/PixelJoint.cpp` — Demonstrates `ImageProvider`
  integration and custom REST routes.

Two known TODOs in the codebase (do not duplicate):
- `plugins/AudioVisualizer/matrix/scenes/AudioSpectrumScene.h:29` — memory leak
  comment on a `base_color` property
- `plugins/Shadertoy/desktop/ShadertoyDesktop.cpp:125` — "rendering twice may
  cause issues" comment

Read `FEATURES.md` and `AGENTS.md` for design constraints before brainstorming.

## Step 2 — Read the rpi-rgb-led-matrix source/docs

The project depends on the **rpi-rgb-led-matrix** library (fork at
`sshcrack/rpi-rgb-led-matrix`, upstream `hzeller/rpi-rgb-led-matrix`). It
provides the hardware abstraction that every scene renders onto.

**How to obtain it locally:**

```bash
# After a cmake configure, the headers are under:
find emulator_build/vcpkg_installed -name "led-matrix.h" 2>/dev/null

# Or use the overlay port source reference:
cat cmake/overlay-ports/rpi-rgb-led-matrix/portfile.cmake

# Alternatively, browse upstream header directly:
# https://raw.githubusercontent.com/sshcrack/rpi-rgb-led-matrix/master/include/led-matrix.h
```

The key header is `include/led-matrix.h` (from the rpi-rgb-led-matrix library).
Read it to discover the rendering API.

### Core Canvas / Matrix
```
include/led-matrix.h
```
- `rgb_matrix::RGBMatrix` / `rgb_matrix::RGBMatrixBase` — The matrix hardware
  handle. Created via `MatrixFactory::CreateMatrix()`. Provides `width()`,
  `height()`, `CreateFrameCanvas()`, `SwapOnVSync()`.
- `rgb_matrix::FrameCanvas` — Offscreen canvas for double-buffered rendering.
  Methods: `SetPixel(x, y, r, g, b)`, `GetPixel()`, `Fill()`, `Clear()`.
- `rgb_matrix::Canvas` — Base class with same pixel API.
- `rgb_matrix::Color` — `{r, g, b}` uint8 struct.
- `rgb_matrix::MatrixFactory` — Parses CLI flags (`--led-rows`, `--led-chain`,
  etc.) and creates the matrix instance. Flags also control hardware timing,
  GPIO mapping, multiplexing, brightness, PWM bits, refresh rate.
- `rgb_matrix::Font` / `rgb_matrix::DrawText()` — Bitmap font rendering
  (used only for the fallback error state; no readable text on the display).

### Frame / Animation Utilities
```
include/framebuffer-internal.h  (library internals — usually not needed)
```
The library handles VSync timing, panel chaining, and pixel mapping internally.
Scenes only need `FrameCanvas::SetPixel` and `RGBMatrix::SwapOnVSync`.

## Step 3 — Suggest 5–8 features

For each feature:

1. **Short slug** (e.g. `feat/...`)
2. **One-sentence summary**
3. **~3 bullet points** of what the implementation would touch
4. **rpi-rgb-led-matrix API references** — specifically cite the classes/
   methods that would be used
5. **Grounding check** — "This makes sense because [reason]" or "This conflicts
   with [existing feature] — skip"

## Rules

- Do NOT suggest features that already exist (check carefully against the 30+
  existing scenes, 2 post-processing effects, 8 transitions, and all REST endpoints)
- Do NOT suggest features that conflict with existing implementation patterns
  (the property system, plugin lifecycle, etc.)
- Ground every feature in actual rpi-rgb-led-matrix API classes you found
- Respect design constraints from `FEATURES.md`: low compute, non-interactive,
  no readable text
- Avoid ideas explicitly rejected in `FEATURES.md` (Home Assistant, WebSocket
  preview, RSS/scores/tickers, scrolling text, web paint, emoji signals)
- Output ONLY the feature list — no preamble, no summary
