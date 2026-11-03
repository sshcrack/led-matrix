# Tier 1 - High Priority

## A1. Global mutable state

Still extern across translation units:

| Global | Declared in |
|--------|-------------|
| `config` (`Config::MainConfig*`), `skip_image`, `exit_canvas_update` | `shared/matrix/include/shared/matrix/utils/shared.h` |
| `Server::registry`, `registryMutex`, `desktop_connection_count`, `currScene`, `currSceneMutex` | `shared/matrix/include/shared/matrix/server/common.h` |
| `Constants::width/height`, `global_post_processor` (raw), `global_transition_manager` (raw) | `shared/matrix/include/shared/matrix/canvas_consts.h` |
| `Constants::global_update_manager` (shared_ptr) | `shared/matrix/include/shared/matrix/utils/consts.h` |
| `interrupt_received`, `interrupt_signal` | `shared/matrix/include/shared/matrix/interrupt.h` |

Fix (incremental): wrap into an `AppContext` constructed in `src_matrix/daemon.cpp` and passed to routes/plugins; remove the extern declarations.

## F2. TOCTOU on `global_post_processor`

`src_matrix/server/post_processing_routes.cpp` (lines ~95-165) reads the raw `Constants::global_post_processor` per handler; `daemon.cpp` nulls it at shutdown while HTTP handlers may still run. Fix: make it a `std::shared_ptr` with atomic load, or stop the server before clearing it.
