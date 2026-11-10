# Tier 3 - Low Priority

- `src_matrix/udp.h` / `udp.cpp` hold `UdpServer`; rename to match the class.
- Add one-line doc comments to non-obvious public declarations with side effects (e.g. `schedule_management.h`).
- Magic numbers: per-scene timing constants in `plugins/AmbientScenes/scenes/` are still inline.
- C++ formatting has no CI check; evaluate a scoped check before reformatting existing code. Web lint already runs in CI.
