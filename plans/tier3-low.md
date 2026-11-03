# Tier 3 - Low Priority

- `src_matrix/udp.h` / `udp.cpp` hold `UdpServer`; rename to match the class.
- Add one-line doc comments to non-obvious public declarations with side effects (e.g. `schedule_management.h`).
- Magic numbers: per-scene timing constants in `plugins/AmbientScenes/scenes/*.cpp` are still inline.
- `.clang-format` and `.prettierrc` exist but no CI step checks formatting.
