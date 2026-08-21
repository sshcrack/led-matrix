# Tier 1 — High Priority

Architectural debt, maintainability blockers, and significant code quality issues.

---

## Group A: Global State & Ownership

### A1. Global mutable state epidemic

**Problem:** The codebase relies on ~15 extern globals shared across translation units:

| Global | Type | File origin |
|--------|------|-------------|
| `config` | `Config::MainConfig*` (raw) | `src_matrix/main.cpp` |
| `interrupt_received` | `std::atomic<bool>` | `shared/matrix/interrupt.h` |
| `exit_canvas_update` | `std::atomic<bool>` | `shared/matrix/utils/shared.h` |
| `skip_image` | `std::atomic<bool>` | (same) |
| `Server::registry` | `std::map<...>` | `shared/matrix/server/common.h` |
| `Server::registryMutex` | `std::shared_mutex` | (same) |
| `Server::currScene` | `std::shared_ptr<Scene>` | (same) |
| `Server::desktop_connection_count` | `std::atomic<int>` | (same) |
| `Constants::width/height` | `std::atomic<int>` | `shared/matrix/utils/consts.h` |
| `Constants::global_post_processor` | `PostProcessor*` (raw) | (same) |
| `Constants::global_transition_manager` | `TransitionManager*` (raw) | (same) |
| `Constants::global_update_manager` | `Update::UpdateManager*` (raw) | (same) |

**Impact:** Every module couples through shared globals. Impossible to unit test anything without running the full system. Thread reasoning is extremely difficult.

**Fix (incremental approach):**
1. Wrap all globals into a single `AppContext` struct/class.
2. Construct `AppContext` in `main.cpp` and pass it (or a reference) to functions that need it.
3. Make `PluginManager` accept an `AppContext&` so plugins don't reach for globals.
4. Remove extern declarations from headers; expose them through `AppContext` getters.
5. This enables testing: create a test-only `AppContext` with mocks.

**This is the single highest-impact refactor in the codebase.**

---

### A2. Custom-deleter `unique_ptr` pattern everywhere

**Problem:** ~15 files use `std::unique_ptr<T, void(*)(T*)>` with raw `new`/`delete` — doubling the unique_ptr size and preventing `make_unique`.

```cpp
// Current pattern (appears ~15 times):
std::unique_ptr<Scene, void (*)(Scene *)>(new XxxScene(), [](Scene *s) { delete s; })

// Also in factory return types, registries, etc.
```

**Fix:**
1. Add a virtual destructor to base classes: `Scene`, `PostProcessingEffect`, `TransitionEffect`, `Post`, `DesktopPlugin`.
2. Where the base class already has a virtual destructor: just use `std::make_unique<T>()` returning `std::unique_ptr<BaseType>`.
3. Remove the custom-deleter overload from all factory functions and registries.
4. This simplifies every plugin's `create_scenes()`, `create_effects()`, etc.

**Files to touch:**
- `shared/matrix/include/shared/matrix/Scene.h`
- `shared/matrix/include/shared/matrix/post_processor.h`
- `shared/matrix/include/shared/matrix/transition_manager.h`
- `shared/matrix/include/shared/matrix/transition_effect.h`
- `shared/matrix/include/shared/matrix/plugin/main.h`
- `shared/matrix/include/shared/matrix/plugin/property.h`
- `shared/matrix/include/shared/matrix/PropertyMacros.h`
- `shared/matrix/include/shared/matrix/wrappers.h`
- `shared/desktop/include/shared/desktop/UdpSender.h`
- `shared/desktop/include/shared/desktop/plugin/main.h`
- Every plugin's `main.cpp` factory function (~15 files)

---

### A3. `PluginRegistry` uses `void*` type erasure

**File:** `shared/matrix/src/plugin_registry.cpp`

**Problem:** The entire registry stores `void*` pointers. A mismatched key returns garbage with no compiler diagnostics.

**Fix:** Replace with `std::any` or use `std::type_index` as the key for fully typed access. Keep the API ergonomic for the current callers.

---

## Group B: Split Overlarge Files & Functions

### B1. `MainConfig` constructor does too much

**File:** `shared/matrix/src/config/MainConfig.cpp:148-227` (80 lines)

**Problem:** Constructor handles: file I/O, JSON parsing, preset migration, UUID generation, schedule migration, config saving. Should decompose into:
- `load_from_file()`
- `migrate_presets()`
- `migrate_schedules()`
- `save()` is already a method, but it's called from the constructor — move the save call out.

---

### B2. `update_canvas()` is 159 lines

**File:** `src_matrix/matrix_control/canvas.cpp:216-375`

**Problem:** Single function handles scene selection, initialization Phase 0, Phase 1 rendering, Phase 2 transitions, post-processing, and emulator Render() calls.

**Fix:** Extract methods:
- `select_and_initialize_scene()`
- `render_phase_one()`
- `render_transition_phase()`
- `apply_post_processing()`
- `render_emulator_frame()` (for the duplicated `#ifdef ENABLE_EMULATOR` block)

---

### B3. `WeatherScene.cpp` is 1361 lines

**File:** `plugins/WeatherOverview/matrix/scenes/WeatherScene.cpp`

**Problem:** The largest file in the codebase. Over a dozen render methods, particle systems, per-frame HTTP requests, SHA-256 hashing on every render.

**Fix:** Split into:
- `WeatherScene.cpp` — main scene logic, render dispatch
- `weather_rendering.cpp` — cloud, lightning, fog, aurora, rainbow rendering
- `weather_particles.cpp` — rain, snow, particle system
- `weather_cache.cpp` — icon download/cache management

Also: move HTTP and filesystem I/O out of the render loop into a background refresh mechanism.

---

### B4. `SortingVisualizerScene.cpp` is 984 lines

**File:** `plugins/AmbientScenes/matrix/scenes/SortingVisualizerScene.cpp`

**Problem:** 34 member variables tracking per-algorithm state. All 10 algorithms in one class with a giant switch statement in `render()`.

**Fix:** Extract each sorting algorithm into its own class/struct implementing a common `SortAlgorithm` interface. The scene then selects and delegates.

---

### B5. `src_desktop/main.cpp::run_app()` is 458 lines

**File:** `src_desktop/main.cpp:176-633`

**Problem:** Everything — logging, asset resolution, plugins, signal handlers, ImGui setup, the GUI lambda (193 lines), cleanup — in one function.

**Fix:** Extract:
- `setup_logging()`
- `resolve_asset_directories()`
- `init_single_instance()`
- `load_and_init_plugins()`
- `create_gui_function()` (extract the 193-line lambda)
- `setup_tray_and_run()`
- `cleanup_and_shutdown()`

---

## Group C: Code Duplication

### C1. Triple duplication in scene management routes

**File:** `src_matrix/server/scene_management.cpp:29-128`

**Problem:** `/list_scenes`, `/list_providers`, `/list_shader_providers` are ~100 lines of nearly identical code. Each iterates over plugin items, gets properties, dumps to JSON. The only differences are the source list and extra output fields.

**Fix:** Create a generic helper:
```cpp
auto list_items_as_json(auto& items, auto extra_fields_fn) -> json;
```
Then each route is a one-liner.

---

### C2. Triple duplication in post-processing routes

**File:** `src_matrix/server/post_processing_routes.cpp:13-163`

**Problem:** `/flash`, `/rotate`, and `/post_processing/effect/:name` all parse `duration`/`intensity` query params identically (including clamping ranges). Also all have empty `catch(...)` blocks suppressing exceptions.

**Fix:** Extract helper:
```cpp
struct DurationIntensity { float duration; float intensity; };
auto parse_duration_intensity(auto& req, float dur_default, float dur_min, float dur_max,
                              float int_default, float int_min, float int_max) -> DurationIntensity;
```
Log exceptions instead of silently swallowing.

---

### C3. `hsl_to_rgb` / `hsv_to_rgb` copy-paste epidemic

**Files (5+ copies):**
- `plugins/AmbientScenes/matrix/scenes/BoidsScene.cpp:8-23`
- `plugins/AmbientScenes/matrix/scenes/FallingSandScene.cpp:9-24`
- `plugins/AmbientScenes/matrix/scenes/NeonTunnelScene.cpp:8-24`
- `plugins/AmbientScenes/matrix/scenes/SortingVisualizerScene.cpp:70-102`
- `plugins/FractalScenes/matrix/scenes/JuliaSetScene.cpp:90-106`
- `plugins/FractalScenes/matrix/scenes/WavePatternScene.cpp:121-137`
- `plugins/GameScenes/matrix/scenes/SnakeGameScene.cpp` (inline HSV in `renderWin`)

**Fix:** Create `shared/matrix/include/shared/matrix/utils/color.h` with `hsl_to_rgb()`, `hsv_to_rgb()`, and replace all copies with `#include` + call.

---

### C4. Transition plugin: 8 duplicated `apply()` methods

**File:** `plugins/Transitions/matrix/Transitions.cpp`

**Problem:** All 8 transitions have an identical triple-nested pixel loop (`from->GetPixel`, `to->GetPixel`, `dst->SetPixel`) differing only in the alpha/weight calculation.

**Fix:** Factor into a template or callback-based helper:
```cpp
void apply_transition(Canvas* from, Canvas* to, Canvas* dst,
                      std::function<uint8_t(uint8_t a, uint8_t b, float t)> blend_fn);
```

---

## Group D: Build System

### D1. `CMAKE_INSTALL_INCLUDEDIR` used before `include(GNUInstallDirs)`

**Files:** `shared/matrix/CMakeLists.txt:45`, `shared/desktop/CMakeLists.txt:51`

**Problem:** Generator expressions reference `${CMAKE_INSTALL_INCLUDEDIR}` at parse time, but `include(GNUInstallDirs)` is called ~40 lines later. The variable is empty, so the install interface degrades to nothing.

**Fix:** Move `include(GNUInstallDirs)` to the top of each file, or use `$<INSTALL_INTERFACE:include>` literally.

---

### D2. GitHub Actions: massive duplication between ci.yml and create-release.yml

**Files:** `.github/workflows/ci.yml`, `.github/workflows/create-release.yml`

**Problem:** The build job in `create-release.yml` (lines 106-250) is a near-verbatim copy of `ci.yml`. Any change must be manually propagated.

**Fix:** Extract the build job into a reusable workflow (`.github/workflows/build.yml` with `workflow_call`), then reference it from both files.

---

### D3. `CROSS_COMPILE_ROOT` resolves to `/cross-compile` when unset

**Files:** `.github/workflows/ci.yml:174`, `.github/workflows/create-release.yml:243`

**Problem:** When `requires-toolchain` is false, the ternary returns empty string → `CROSS_COMPILE_ROOT` becomes `/cross-compile`.

**Fix:**
```yaml
CROSS_COMPILE_ROOT: ${{ matrix.requires-toolchain && format('{0}/cross-compile', runner.temp) || '' }}
```

---

### D4. `install_led_matrix.sh`: service runs as root

**File:** `scripts/install_led_matrix.sh:284`

**Problem:** `User=root` in the systemd service. The LED matrix controller doesn't need root privileges.

**Fix:** Change to `User=pi` (or make configurable). Adjust file permissions so `pi` can access the relevant paths.

---

## Group E: React Web

### E1. No error boundary

**File:** `react-web/src/App.tsx`

**Problem:** Any unhandled exception crashes the entire PWA with a white screen.

**Fix:** Add `<ErrorBoundary>` wrapping the `<Routes>` element. Show a "Something went wrong" fallback with a reload button.

---

### E2. TypeScript type safety bypassed with `any`

**Files:**
- `react-web/src/apiTypes/list_presets.ts:7` — `SceneArguments { [key: string]: any }`
- `react-web/src/apiTypes/list_scenes.ts:1` — `Property<any>[]`
- `react-web/src/apiTypes/list_scenes.ts:41` — Provider union with `{ [key: string]: any }` catch-all

**Problem:** Every scene property, argument, and provider value is `any`. The whole API type layer provides zero compile-time safety.

**Fix (incremental):**
1. Replace `Property<any>` with a discriminated union of known property types (`BooleanProperty`, `NumberProperty`, `StringProperty`, `EnumProperty`, etc.).
2. Replace `{ [key: string]: any }` with properly typed interfaces per scene/provider.
3. Aim to remove all `any` from the apiTypes directory.

---

### E3. All pages eagerly imported

**File:** `react-web/src/App.tsx:6-12`

**Problem:** `import Schedules from './pages/Schedules'` — every page module is loaded upfront, increasing initial bundle size.

**Fix:** Use `React.lazy(() => import('./pages/Schedules'))` with `<Suspense fallback={<Loader />}>`.

---

### E4. `noUnusedLocals` and `noUnusedParameters` disabled

**File:** `react-web/tsconfig.json:15-16`

**Problem:** Dead code and unused parameters silently pass compilation.

**Fix:** Enable both, then fix all violations throughout the web codebase.

---

## Group F: Thread Safety

### F1. `udp.cpp` constructor continues after socket creation failure

**File:** `src_matrix/udp.cpp:107-161`

**Problem:** If `socket()` returns -1, the constructor logs an error but continues to run `setsockopt`, `fcntl`, `bind`, and spawns a thread — all on an invalid fd.

**Fix:** Add early returns after each error. Or use RAII wrappers for the socket fd.

---

### F2. `post_processing_routes.cpp`: TOCTOU race on `global_post_processor`

**File:** `src_matrix/server/post_processing_routes.cpp:14` (and every handler)

**Problem:**
```cpp
if (Constants::global_post_processor) {
    Constants::global_post_processor->add_effect(...);
}
```
The null check and dereference are not atomic. Another thread could delete the processor between the check and the call.

**Fix:** Use a `std::shared_ptr<PostProcessor>` with atomic load, or ensure the processor is never deleted during server operation.

---

### F3. `WebsocketClient`: lambda captures `[this]` stored in `ix::WebSocket`

**File:** `shared/desktop/src/shared/desktop/WebsocketClient.cpp:20-45`

**Problem:** The constructor passes a lambda capturing `[this]` to `webSocket.setOnMessageCallback(...)`. If the WebSocket outlives the `WebsocketClient`, `this` dangles.

**Fix:** Use `std::enable_shared_from_this` and capture a `shared_from_this()` instead of raw `this`.
