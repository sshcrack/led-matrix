# Tier 2 — Medium Priority

Code quality, performance, and build system improvements that don't block correctness but reduce maintenance burden.

---

## Build System

### B1. Root `CMakeLists.txt` has dead-code nested condition

**File:** `CMakeLists.txt:302-323`

**Problem:** Line 302 `if(NOT ENABLE_DESKTOP)` wraps an inner `if(NOT ENABLE_DESKTOP)` at line 308. The outer already guarantees the condition — the inner is dead code.

**Fix:** Remove the redundant inner `if`.

---

### B2. Root `CMakeLists.txt`: GLOB for source collection

**File:** `CMakeLists.txt:115-126`

**Problem:** `file(GLOB_RECURSE SOURCES ...)` — new `.cpp` files aren't automatically detected without a reconfigure.

**Fix:** List sources explicitly, or keep the GLOB but add a `configure_file` touch to force re-configure when sources change.

---

### B3. `cmake/update_package_version.cmake` has React Native copypasta

**File:** `cmake/update_package_version.cmake:2,126-128`

**Problem:** "Script to update React Native package.json" and `react_native_install` target references. This project is Vite + React, not React Native.

**Fix:** Remove the comments and dead code.

---

### B4. `cmake/vcpkg_features.cmake` does string-based JSON parsing

**File:** `cmake/vcpkg_features.cmake:17`

**Problem:** `string(FIND ... "\"${FEATURE_NAME}\":" ...)` — searches for JSON key as a substring. False positive if the feature name appears in a description or as a substring of another feature name.

**Fix:** Use proper JSON parsing (CMake's `json` command or pass via file) or at least anchor the search with boundary checks.

---

### B5. `.clangd` hardcodes `emulator_build/`

**File:** `.clangd:2`

**Problem:** Only works for the emulator preset. Desktop/cross-compile developers must edit or symlink.

**Fix:** Document in AGENTS.md, or use a relative `.clangd` per build dir, or use `compile_commands.json` symlinks.

---

### B6. `cmake_minimum_required(VERSION 3.5.0)` is too low

**File:** `CMakeLists.txt:1`

**Problem:** 3.5.0 is from 2016. The project uses C++23, CMakePresets v6, generator expressions. Should be at least 3.21, ideally 3.26.

**Fix:** Bump to 3.21 minimum (3.26 preferred) and verify all CMake still works.

---

## C++ Code Quality

### C1. `using namespace std;` in headers

**Files:** Every `src_matrix/server/*.h`

**Problem:** Namespace pollution forces `std::` namespace on all includers — a significant C++ best-practice violation.

**Fix:** Remove `using namespace std;` from all headers. Fully qualify types (e.g., `std::string`, `std::vector`).

---

### C2. C-style casts

**Files with instances:**
- `shared/matrix/src/Scene.cpp:50` — `((FallbackScene*) ...)`
- `shared/matrix/src/config/data.cpp:47` — `(const Scenes::Scene *&)`
- `src_matrix/matrix_control/canvas.cpp:252,299,363` — `((EmulatorMatrix*) ...)`
- `src_matrix/udp.cpp:24,155` — `(struct sockaddr *)`
- `plugins/AmbientScenes/matrix/AmbientScenes.cpp` deleters (lines 27, 32, 37, 42, 47, 52, 57, 62, 67)
- `plugins/AmbientScenes/matrix/scenes/DigitalRainScene.cpp:71-73`

**Fix:** Replace with `static_cast`, `reinterpret_cast` (for sockaddr), or `dynamic_cast` as appropriate.

---

### C3. `std::localtime` is not thread-safe

**Files:**
- `shared/matrix/src/config/data.cpp:216`
- `plugins/AmbientScenes/matrix/scenes/ClockScene.cpp:21,32,255,679`

**Problem:** `std::localtime(&t)` returns a pointer to internal static storage — not thread-safe.

**Fix:** Use `localtime_r` (POSIX) or `localtime_s` (C11/Windows).

---

### C4. File-scope globals in `canvas.cpp`

**File:** `src_matrix/matrix_control/canvas.cpp:25-28`

**Problem:** Four file-scope globals (`ERROR_COLOR`, `ERROR_FONT`, `load_font_error`, `loaded_font_success`) used by `render_fallback()`. They're hidden mutable state.

**Fix:** Make them `static` file-scope or better, move them into the function that uses them.

---

### C5. Self-assignment dead code

**Files:**
- `plugins/AmbientScenes/matrix/scenes/BoidsScene.cpp:28-29`
- `plugins/AmbientScenes/matrix/scenes/SortingVisualizerScene.cpp:692-693`

**Problem:** `matrix_width = matrix_width;` — doesn't read from the parent `Scene::initialize()` call. Dead code.

**Fix:** Remove the redundant assignments (the member was already set by the base class `Scene::initialize()`).

---

### C6. `rand()` without seeding

**Files:** Multiple plugin scenes use C `rand()` without calling `srand()`:
- `BoidsScene.cpp`, `FallingSandScene.cpp`, `StarFieldScene.cpp`, `WeatherScene.cpp`, `GameOfLifeScene.cpp`, `WavePatternScene.cpp`, `MazeGameScene.cpp`, etc.

**Fix:** Replace with `<random>` and stored `std::mt19937` engine members:
```cpp
// In class: std::mt19937 rng{std::random_device{}()};
// Usage: std::uniform_int_distribution{0, max}(rng);
```

---

### C7. Frame-rate-dependent timing

**Files assuming 60 FPS:**
- `MetaBlobScene.cpp:117` — `time += 1.0f / 60.0f`
- `NeonTunnelScene.cpp:32` — `time_counter += 0.05f`
- `PingPongGameScene.cpp:59-60` — `* time_step * 60.0f`
- `SnakeGameScene.cpp:403` — `frame_counter * 0.016f`
- `WeatherScene.cpp:789` — `p.life_time += 0.016f`

**Fix:** Use the scene's `delta_time` (or `get_frame_duration()`) instead of hardcoded 1/60.

---

### C8. `WeatherScene` does per-frame HTTP + SHA-256

**File:** `plugins/WeatherOverview/matrix/scenes/WeatherScene.cpp:571-689`

**Problem:** Every render frame:
1. Calls `parser.get_data()` (HTTP request to weather API)
2. Downloads and caches weather icons
3. Computes SHA-256 of every icon URL (`picosha2::hash256_hex_string`)

**Fix:** Add a refresh timer — only re-fetch weather data every N minutes. Cache icon data in memory between renders. Only hash when the URL changes.

---

### C9. `WeatherScene` uses `static` locals for fog/rainbow/aurora state

**File:** `plugins/WeatherOverview/matrix/scenes/WeatherScene.cpp:1170-1327`

**Problem:** `static float fog_time`, `static bool fog_initialized`, `static std::vector fog_patches`, etc. Multiple instances share state — cross-instance contamination.

**Fix:** Move to member variables. Only use static for truly global caches.

---

### C10. `VideoStreamEngine.cpp` (641 lines) has duplicated subprocess logic

**File:** `shared/desktop/src/shared/desktop/VideoStreamEngine.cpp`

**Problem:** The `play_fast_chunk()` and `download_and_process_chunk()` methods both construct near-identical `yt-dlp` and `ffmpeg` command pipelines. The `run_command()` function is duplicated for Windows/POSIX.

**Fix:** Factor command construction into `build_ytdlp_command()`, `build_ffmpeg_command()`. Factor `run_command()` into a shared utility (could go in `shared/desktop/utils.h`).

---

### C11. `SortingVisualizerScene` has unnecessary mutex

**File:** `plugins/AmbientScenes/matrix/scenes/SortingVisualizerScene.cpp:30,123-731`

**Problem:** `access_indices_mutex` protects `access_indices`, but the scene runs on a single thread — the mutex is never contended.

**Fix:** Remove the mutex and associated lock/unlock calls.

---

### C12. Custom assets: manual multipart parser reimplements the wheel

**File:** `src_matrix/server/custom_assets_management.cpp:40-112`

**Problem:** 72-line manual multipart/form-data parser with string searching, boundary detection, filename extraction. Error-prone and a security concern (unbounded memory allocation from request body).

**Fix:** If restinio provides multipart parsing, use it. Otherwise, harden the parser: add max file size limits, ensure null-termination, validate boundary format, and add explicit error messages.

---

### C13. Desktop app: `run_app()` `static` variables for non-static state

**File:** `src_desktop/main.cpp:225-227,261-267,279,294,311`

**Problem:** Function-local `static` variables (`ws`, `hostname`, `port`, `fpsLimit`, `udpFpsLimit`) that are semantically per-instance. They persist after `run_app()` returns and won't reflect config reloads.

**Fix:** Make them local (non-static) and pass them explicitly to the GUI lambda.

---

### C14. Desktop app: `std::thread(...).detach()` in `change_matrix_status()`

**File:** `src_desktop/main.cpp:137-161`

**Problem:** Fire-and-forget thread. If the HTTP request hangs, the thread is leaked. No cancellation mechanism.

**Fix:** Use `std::async` with a timeout, or keep a joinable thread, or use `cpr`'s built-in timeout.

---

### C15. Inconsistent API URL conventions

**Files:** `src_matrix/server/*.cpp`

**Problem:** Some routes use `/api/` prefix (custom-assets, updates), others don't (`/set_active`, `/list_scenes`, `/flash`). Some GET endpoints have side effects (`/set_active` modifies state).

**Fix:** Pick one convention (either all under `/api/` or none) and stick with it. Make state-mutating endpoints use POST/PUT/DELETE.

---

## Desktop App Specific

### D1. `run_app()` `static bool shouldExit` not `volatile sig_atomic_t`

**File:** `src_desktop/main.cpp:64`

**Problem:** Already in Tier 0 — but also affects Windows path (line 76-95) where `console_ctrl_handler` writes to non-atomic `bool`.

**Fix:** Use `std::atomic<bool>` for all inter-thread/interrupt flags.

---

### D2. Hostname filter blocks IPv6 addresses

**File:** `src_desktop/filters.cpp:6,13`

**Problem:** The hostname input filter doesn't allow `:` (colon), so IPv6 addresses like `::1` or `2001:db8::` cannot be entered.

**Fix:** Add `:` to the allowed character set, or detect IPv6 format and validate separately.

---

## React Web

### E1. `lodash` as full dependency

**File:** `react-web/package.json:27`

**Problem:** `lodash` is large. Tree-shaking with CommonJS modules is poor.

**Fix:** Switch to `lodash-es` for proper tree-shaking, or import individual functions directly.

---

### E2. `target: "ES2020"` is outdated

**File:** `react-web/tsconfig.json:3`

**Problem:** Modern browsers support ES2022+. Vite handles downleveling.

**Fix:** Bump to `ES2022` — produces smaller, faster output for modern browsers.

---

### E3. PWA screenshots not verified at build time

**File:** `react-web/vite.config.ts:20-34`

**Problem:** References `icon-192.png`, `screenshot-wide.png`, `screenshot-mobile.png` but no build-time check.

**Fix:** Add a build step or pre-commit hook that verifies these files exist and have the right dimensions.

---

### E4. Layout uses fragile route matching

**File:** `react-web/src/components/Layout.tsx:61`

**Problem:** `location.pathname.startsWith('/modify-')` — matches `/modify-foo`, `/modify-anything`, etc.

**Fix:** Use React Router's `useMatch` or a route config boolean for exact matching.

---

## `src_preview_gen`

### F1. Preview generator is tightly coupled to emulator

**File:** `src_preview_gen/main.cpp`

**Problem:** Only builds under the emulator preset. Relies on `ENABLE_EMULATOR` and `EmulatorMatrix`.

**Fix:** If the preview generator is useful standalone, extract the frame rendering from the emulator dependency. If not, document its limitations in AGENTS.md.
