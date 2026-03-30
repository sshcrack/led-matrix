# WASM Browser-Side Scene Preview

This directory contains the WebAssembly build that enables **live scene previews** to run
entirely in the user's browser without touching the LED matrix hardware.

Selected scenes from the `ExampleScenes` plugin are compiled with
[Emscripten](https://emscripten.org) into a single `scene_preview.wasm` module. The React
web application loads the module inside a Web Worker and renders frames via
`OffscreenCanvas` / `ImageData` at ~15 FPS.

---

## Architecture

```
wasm/
├── include/                        # Platform-specific header shims
│   ├── canvas.h                    # Minimal rgb_matrix::Canvas interface
│   ├── graphics.h                  # rgb_matrix::Color + stub draw functions
│   ├── led-matrix.h                # FrameCanvas (RGBA buffer) + RGBMatrix stubs
│   ├── thread.h                    # Empty threading stub
│   ├── pixel-mapper.h              # Empty pixel-mapper stub
│   ├── Magick++.h                  # Empty stub (satisfies #include directives)
│   └── shared/matrix/
│       ├── utils/utils.h           # Matrix utils without Magick++ / filesystem
│       ├── post.h                  # Minimal Post stub (no image loading)
│       ├── canvas_consts.h         # Forward-declare PostProcessor/TransitionManager
│       └── plugin/
│           └── TransitionNameProperty.h  # Simplified (no TransitionManager)
├── runtime/
│   ├── wasm_runtime.cpp            # extern "C" scene preview API
│   ├── wasm_scene.cpp              # Scene.cpp replacement (no PluginManager)
│   └── wasm_stubs.cpp              # Definitions for stubs & globals
└── CMakeLists.txt                  # Emscripten build target
```

### JavaScript API

| Function | Signature | Description |
|---|---|---|
| `wasm_list_scenes` | `() → const char*` | JSON array of scene metadata |
| `wasm_scene_create` | `(name, w, h) → int` | 0 = ok, non-zero = not found |
| `wasm_scene_set_properties` | `(json) → void` | Update scene properties |
| `wasm_scene_render_frame` | `() → int` | Render one frame; 1=continue, 0=stop |
| `wasm_scene_destroy` | `() → void` | Release current scene |
| `wasm_get_frame_buffer` | `() → uint8_t*` | RGBA buffer pointer in WASM heap |
| `wasm_get_buffer_size` | `() → int` | Buffer byte length |

---

## Prerequisites

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (emsdk ≥ 3.1)
- CMake ≥ 3.15
- Internet access (FetchContent downloads `nlohmann/json`, `magic_enum`, `spdlog`, `fmt`)

---

## Building

```bash
# Activate emsdk (adjust path as needed)
source ~/emsdk/emsdk_env.sh

# Configure from the repository root:
mkdir wasm_build && cd wasm_build
emcmake cmake ../wasm -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Install artefacts into react-web/public/wasm/
cmake --install .
```

After a successful build, the following files are placed in
`react-web/public/wasm/`:

```
react-web/public/wasm/
├── scene_preview.js    ← Emscripten loader (ES module factory)
└── scene_preview.wasm  ← WASM binary
```

The Vite dev server and production build will serve these as static assets under
`/web/wasm/`.

---

## Adding more scenes to the WASM build

v1 includes only the `ExampleScenes` plugin (pure canvas math, no heavy deps).
To add a new scene:

1. **Check dependencies**: the scene's `.h` / `.cpp` must not include anything that
   transitively pulls in `Magick++`, `restinio`, file I/O requiring POSIX, or native
   audio/video APIs.
2. **Add the scene sources** to `WASM_SOURCES` in `wasm/CMakeLists.txt`.
3. **Register the wrapper** in `wasm/runtime/wasm_runtime.cpp`'s `ensure_registry()`.
4. **Add the scene name** to `WASM_SUPPORTED_SCENES` in
   `react-web/src/components/WasmScenePreview.tsx` so the frontend uses live
   rendering instead of the GIF fallback.

---

## Fallback strategy

The React component (`WasmScenePreview`) uses **GIF fallback** when:

- `sceneName` is not in `WASM_SUPPORTED_SCENES` (e.g. desktop-dependent scenes).
- The WASM module fails to load (not yet built, network error, etc.).
- `wasm_scene_create()` returns non-zero (scene not found in the module).
- A runtime error is thrown by the Web Worker.

This ensures the UI remains fully functional before the WASM artefacts are built.
