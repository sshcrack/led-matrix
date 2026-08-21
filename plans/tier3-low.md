# Tier 3 — Low Priority

Style, naming, and minor cleanups. Safe for new contributors or automation.

---

## Duplicate Includes

| File | Line | Detail |
|------|------|--------|
| `src_matrix/main.cpp` | 17 (duplicate of 5) | `#include "shared/matrix/utils/shared.h"` |
| `src_matrix/main.cpp` | 39 (duplicate of 10) | `#include "shared/matrix/utils/consts.h"` |

**Fix:** Remove the duplicate lines.

---

## Unused Includes

| File | Line | Detail |
|------|------|--------|
| `src_matrix/main.cpp` | 7 | `#include <utility>` — not directly used |
| `src_matrix/matrix_control/canvas.h` | 4 | `#include "content-streamer.h"` |
| `src_matrix/matrix_control/canvas.h` | 6 | `#include "shared/matrix/post.h"` |
| `src_matrix/matrix_control/canvas.h` | 7 | `#include "shared/matrix/utils/canvas_image.h"` |
| `shared/desktop/src/UpdateManager.cpp` | 8 | `#include "hello_imgui/hello_imgui.h"` — not used |
| `shared/desktop/src/VideoStreamEngine.cpp` | 2 | `#include "shared/desktop/utils.h"` — not used in this file |
| `vcpkg.json` | 6 | `picosha2` — never referenced in any CMakeLists.txt |

**Fix:** Remove unused includes and unused vcpkg dependency.

---

## Stale / Orphaned Comments

| File | Line | Comment |
|------|------|---------|
| `src_matrix/main.cpp` | 133-134 | `// Should be in hardware.cpp but this actually drops privileges, so I moved it here` — refers to code that was already moved/removed |
| `src_matrix/udp.cpp` | 76 | `(note: using data[1] as magicPacket for backward compatibility)` — references a protocol design detail no longer evident from code |
| `shared/matrix/include/plugin/property.h` | 217 | `// If invalid, keep default value and log warning (could add logging here)` — unimplemented TODO as a comment |
| `shared/matrix/include/Scene.h` | 31 | `// Pure virtual methods remain unchanged` — self-evident |
| `cmake/update_package_version.cmake` | 134-172 | 37 lines of instructional documentation — belongs in README or a comment block, not inline |
| `plugins/Video/matrix/scenes/VideoScene.cpp` | 119 | `// This block was missing in the snippet, added to handle initial selection` — documents a bug fix, not relevant in production |
| `plugins/AmbientScenes/matrix/scenes/BoidsScene.cpp` | 45-46 | `// Boids move fast, let's clear the canvas completely to avoid mess? Or fade. Let's clear for now.` — unresolved design decision |

**Fix:** Remove stale comments. Move the useful ones (documenting why a decision was made) to a cleaner format, or convert the TODO to an actual task.

---

## Inconsistent Naming

| Location | Issue |
|----------|-------|
| `src_matrix/matrix_control/hardware.cpp:22` vs `canvas.cpp:216` | `composite_offscreen_Canvas` (capital C) vs `composite_offscreen_canvas` (lowercase c) — across files |
| `shared/matrix/include/Scene.h:92` | `getCategory()` uses PascalCase, every other method uses `snake_case` |
| `src_matrix/server/update_routes.cpp:87` | `parsed_version.isInvalid()` — PascalCase method in a snake_case codebase |
| `src_desktop/filters.h:4` | `HostnameFilter` — PascalCase function in snake_case codebase |
| `src_matrix/server/*.cpp` | Routes: some under `/api/`, some not; `/set_active` is GET with side effects |
| `src_matrix/udp.h` | Class is `UdpServer` but file is `udp.h` (should be `UdpServer.h`) |

**Fix:** Rename to match project convention (snake_case for functions/methods).

---

## Hardcoded Magic Numbers

Found across the codebase — these should be named constants:

| File | Value | What |
|------|-------|------|
| `src_matrix/main.cpp:82-83` | `256 * 1024 * 1024`, `512 * 1024 * 1024` | Memory limits |
| `src_matrix/udp.cpp:11,131` | `64 * 1024`, `256 * 1024` | Buffer sizes |
| `src_matrix/udp.cpp:46-49,67,86` | `7` | Packet header size |
| `src_matrix/matrix_control/canvas.cpp:182-183` | `33`, `140` | Min/max render interval (ms/FPS) |
| `shared/matrix/src/Server/common.cpp:5` | `8080` | Default HTTP port |
| `src_matrix/server/post_processing_routes.cpp:18-19,59,65,74` | `0.5f, 1.0f, 0.1f, 5.0f, 0.0f, 1.0f, 0.5f, 10.0f, 2.0f` | Default durations/intensities with clamp ranges |
| `plugins/AmbientScenes/scenes/*.cpp` | Various `0.05f, 0.15f, 0.5f, etc.` | Per-scene timing constants |

**Fix:** Define named constants at the top of the relevant file or in a shared constants header.

---

## File Header Comments for Declaration Files

**Issue:** Several small `.h` files have no documentation of what the function/class does. This is acceptable for trivial wrappers but some (e.g., `canvas_status.h`, `schedule_management.h`) export functions with non-obvious side effects.

**Fix (lowest priority):** Add one-line doc comments to non-obvious public function declarations.

---

## `.gitignore` improvements

**Issue:** Build directories (`build/`, `emulator_build/`, `desktop_build/`) are covered by existing rules, but there are no entries for:
- IDE files beyond `.idea/` (e.g., `*.swp`, `*.swo`)
- Compiled plugin `.so`/`.dll` files in install directories

**Fix:** Add common editor/IDE ignores and any generated binary patterns.

---

## Codify Code Style

**Issue:** No `.clang-format` or `.prettierrc` in the repo. Code consistency relies on developer discipline.

**Fix:** Generate a `.clang-format` matching the current code style (2-space indent, snake_case functions, etc.) and a `.prettierrc` for TypeScript. Add a CI step to check formatting.
