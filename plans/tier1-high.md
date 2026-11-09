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
