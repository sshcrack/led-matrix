# Priority 1: global mutable state

Checked 2026-11-10. `Daemon` already owns the config, postprocessor and transition
manager with `unique_ptr`, and the update manager with `shared_ptr`
(`src_matrix/daemon.h`). The remaining problem is access through exported globals,
not missing ownership.

| Global access | Declaration |
|---|---|
| `config`, `skip_image`, `exit_canvas_update` | `shared/matrix/include/shared/matrix/utils/shared.h` |
| WebSocket registry, desktop count, current scene and their locks | `shared/matrix/include/shared/matrix/server/common.h` |
| Matrix dimensions, raw postprocessor and transition-manager aliases | `shared/matrix/include/shared/matrix/canvas_consts.h` |
| `global_update_manager` | `shared/matrix/include/shared/matrix/utils/consts.h` |
| Interrupt flags | `shared/matrix/include/shared/matrix/interrupt.h` |

## First slice

1. Replace direct config/postprocessor/transition-manager global reads in daemon
   consumers with narrow constructor or route-registration dependencies. Keep
   ownership in `Daemon`; avoid a context object exposing every subsystem.
2. Migrate plugin consumers separately, accounting for the shared-library boundary
   and the preview generator and desktop worker, which also instantiate plugins.
   Remove each exported alias once all execution hosts have migrated.
3. Preserve shutdown ordering: stop request and render work, release plugin-owned
   objects and callbacks, then unload plugins and destroy their dependencies.

Completion means the migrated consumers no longer use the corresponding globals,
startup and shutdown pass in the emulator and desktop worker, and partial
initialization still tears down safely. Use the existing plugin-lifecycle,
handoff and renderer regressions; add coverage for any newly exposed lifecycle
boundary. This work should add no per-frame polling, locks or allocations.

Treat socket-registry ownership, current-scene publication and signal-safe
interrupt flags as separate follow-ups with their own synchronization contracts.
