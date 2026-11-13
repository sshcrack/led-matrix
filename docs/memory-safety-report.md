# Memory-safety and threading report

The original `safety/sanitizers` audit is retained below as historical evidence.
Its test counts and verification limits describe that audit, not the current
revision. Tooling and CI status were checked against the tree on 2026-11-10; the
audit's limits were re-verified on 2026-11-13 (see "Gap re-verification").

## Using the tooling

| Preset | Sanitizer | Build dir |
|---|---|---|
| `emulator-asan` | ASan + UBSan (UB aborts) | `emulator_asan_build/` |
| `emulator-tsan` | TSan | `emulator_tsan_build/` |
| `desktop-linux-asan` | ASan + UBSan | `desktop_asan_build/` |
| `desktop-linux-tsan` | TSan | `desktop_tsan_build/` |

```bash
cmake --preset emulator-asan && cmake --build --preset emulator-asan --target install -j3
source scripts/sanitizers/env.sh        # *SAN_OPTIONS, suppressions, OMP_NUM_THREADS=1
./emulator_asan_build/install/bin/led-matrix --led-chain 2 --led-parallel 2 --led-rows 64 --led-cols 64 --led-emulator --led-emulator-headless
ctest --test-dir emulator_asan_build --output-on-failure
```

- `-DLED_MATRIX_SANITIZE=OFF|address|thread` (`cmake/Sanitizers.cmake`) applies to
  every in-tree target including plugin DSOs. ASan and TSan are mutually exclusive.
  The old `ENABLE_SANITIZERS=ON` still works as an alias for `address`.
- `_GLIBCXX_ASSERTIONS` is on in every build including the Pi release.
- Warnings (`-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wnull-dereference
  -Wuse-after-free=3`; `/W4` on MSVC) apply to our targets only (`cmake/Warnings.cmake`);
  `-DLED_MATRIX_WERROR=ON` (set by CI for the emulator, desktop-linux, desktop-windows and
  cross-compile builds) makes them errors.
  Disable with `-DLED_MATRIX_WARNINGS=OFF`.
- Suppressions (`scripts/sanitizers/*.supp`) are third-party frames only.
  `env.sh` also sets `OMP_NUM_THREADS=1` (GraphicsMagick/libgomp is uninstrumented)
  and `TSAN_OPTIONS=ignore_noninstrumented_modules=1` (Mesa/GLX teardown).
  Set `SANITIZER_LOG_DIR` to write reports to files.
- Sanitizer builds skip `dlclose()` of plugins so exit-time reports symbolize.
- Core dumps: the crash reporter now re-raises the signal after writing
  `crashes/crash-*.txt`, the systemd unit has `LimitCORE=infinity`; use
  `sudo coredumpctl list|info|debug led-matrix` (install `systemd-coredump`, `gdb`;
  both are installed on `ledmat` since 2026-11-13).

## Historical audit findings

| # | Tool | Location | Bug class | Status |
|---|---|---|---|---|
| 1 | review | `AudioVisualizer` `Recorder::startDefaultOutputLoopback` | Restarting loopback assigned over a joinable `std::thread` -> `std::terminate`; partial pipe reads dropped | Fixed |
| 2 | review | `VideoStreamEngine` (`stop()` vs processing thread) | Double `waitpid` of ffmpeg child (ECHILD / pid reuse); unlocked `last_error_` read | Fixed |
| 3 | ASan/TSan run | `src_matrix/server/desktop_ws.cpp` `/desktopWebsocket` | Registration race: messages delivered before the socket was registered | Fixed (`activation_t::delayed`) |
| 4 | TSan | `UpdateManager::latest_version_` | Data race on `std::string` (update loop vs HTTP handlers) | Fixed (mutex) |
| 5 | UBSan | `Config::General` (desktop) | Load of uninitialized `bool` (copy of indeterminate member) | Fixed (default initializers) |
| 6 | review | `ShadertoyDesktop` `url`/`custom_shader_*`/`hasUrlChanged` | WebSocket thread vs UI thread data race on strings | Fixed (mutex + generation counter; `width`/`height` atomic) |
| 7 | review | `VideoDesktop::current_url`, `size:` parsing | String data race; `std::stoi` throw / missing `x` on malformed peer message | Fixed |
| 8 | TSan | `websocketClientInstance` global | Plain pointer written by main, read by plugin message thread | Fixed (`std::atomic`) |
| 9 | tooling | crash reporter | Handler swallowed the signal, so no core dump | Fixed |
| 10 | TSan | libgomp/GraphicsMagick races | Uninstrumented OpenMP barriers | Suppressed (third party) |
| 11 | TSan | libX11/xcb/GLX/EGL lock-order | Third-party lock order inversions | Suppressed (third party) |
| 12 | TSan | GLFW/Mesa `destroyContextGLX` | "unlock of unlocked mutex" then TSan aborts "nested bug". Reproduces with a bare GLFW program | Worked around (`ignore_noninstrumented_modules=1`); not our code |
| 13 | TSan | `called_from_lib:libpulse` | Suppression matched two libs and aborted every desktop test | Fixed (`libpulse.so.0`) |
| 14 | clang-tidy | whole tree (191 files) | 1859 diagnostics; triaged, none real beyond the above (see below) | No change |

### Clean runs

- ASan+UBSan: emulator tests 39/39; `preview_gen` over all 55 previewable scenes;
  live emulator through all presets and 57 scenes (Scene Lab), concurrent
  `/live_frame` viewers, malformed UDP/HTTP/WebSocket input
  (`drive_emulator.py`-style driver, not committed). Desktop ASan suite 40/41.
- TSan: emulator run (same driver) found only #4. Desktop suite 40/41.
- UDP framing in `src_matrix/udp_server.cpp` and the plugin `on_udp_packet` handlers
  validate sizes before copying (reviewed, no change).

### clang-tidy triage

`bugprone-*`, `concurrency-*`, `cppcoreguidelines-pro-bounds-*`, `clang-analyzer-*`
(minus `easily-swappable-parameters`, `exception-escape`) via a pip clang-tidy 21.
Mostly style noise: `narrowing-conversions` 1216, `pointer-arithmetic` 210,
`constant-array-index` 166, `misplaced-widening-cast` 82. Looked at every
`unchecked-optional-access`, `integer-division`, `incorrect-roundings` and the
`implicit-widening-of-multiplication` hits near untrusted input (UDP, uploads, image
decode, RenderOffload): all benign (bounded constants, predicate-guarded optionals,
property wrappers). `concurrency-mt-unsafe` (getenv/localtime/strerror) not
individually triaged.

### shadertoy-headless library (separate repo)

Built with ASan+UBSan: `shadertoy-library-smoke` passes, `shadertoy-preview`
renders all 11 led-matrix shaders cleanly, and ~6000 mutated `.sttf` files plus
hostile ShaderToy JSON responses through `ShaderDocument::load` /
`importFromShaderToyResponse` produced no sanitizer report (only exceptions). No library
change was needed.

## Original audit limits

- Anything touching real GPIO / the Pi (`/dev/mem`, DMA timing, privilege drop)
  was not run; the arm64 build compiles but nothing was executed on `ledmat`, and
  sanitizers were only run on x64 against the emulator.
- Hardware-backed GL on the desktop (real GPU drivers) was not exercised; shader
  tests ran on Mesa.
- Spotify, Deezer, weather and update-download network paths ran without
  credentials, so only their error paths were exercised.
- `audio_analyzer_benchmark` fails in sanitizer build trees ("cannot open
  libSharedToolsCommon.so": install layout / RPATH). It passes from the installed
  tree. Unfixed, environmental.
- `/live_frame` `next` waits for an updated frame (capture only when the scene
  produced one), so held scenes (e.g. desktop-fed scenes with no desktop) leave a
  viewer waiting. Consistent with the pull-based design; not changed.
- TSan cannot see ordering inside uninstrumented libraries (libgomp, vcpkg
  libraries, Mesa), so races fully inside them are invisible.

## Gap re-verification (2026-11-13)

Each limit above was rechecked against `master` at `5535f30c` (plus the fixes in
the same change) with GCC 15.2 on x64 Linux (Intel Arc/Mesa 26.0.8 iris) and on
`ledmat` (Pi 3B, Raspberry Pi OS bookworm, kernel 6.12, led-matrix 1.22.0 `.deb`).

| Original gap | Current evidence | Result |
|---|---|---|
| Real GPIO / privilege drop | Running daemon on `ledmat` after GPIO init: real, effective, saved and filesystem UID/GID all `pi` (1000). | Privilege drop verified (unsanitized). |
| Core dumps from the Pi | `systemd-coredump` was **not** installed; installed it (`fs.suid_dumpable=2`, pipe `core_pattern`). `kill -SEGV` on the service: crash report written, core dumped, service restarted. `coredumpctl debug` gives a file:line backtrace (`crash_reporter.cpp:577` → `RemoteRender::copy_latest`). A process that dropped privileges *before* `suid_dumpable` became 2 does not dump, so the first dump works after the next service restart. | Verified. Modules lacked GNU build-ids, so `coredumpctl info` could not match them; Linux links now add `--build-id`. |
| Sanitizers on the Pi | Not run. ASan's slowdown would distort the GPIO refresh timing being tested, and it would need replacing the installed service for the run. The Pi-only GPIO/DMA code lives in `rpi-rgb-led-matrix` (Release build, no debug info in the `.deb`). | Still unverified, by choice. |
| Hardware GL | All 10 `plugins/Shadertoy/shaders/*.frag` through `shadertoy_shader_preview` (ASan+UBSan) on the real display: iris submitted GPU batches (`INTEL_DEBUG=submit`; none under `xvfb-run`). `scene_worker_render_smoke` and `audio_visualizer_render_regression` run on the display, too. No reports. The ctest shader smokes themselves still run under `xvfb-run` (llvmpipe). | Verified on Intel iris; NVIDIA/AMD and Windows drivers not covered. |
| Weather / IP location | `network_paths_probe` (ASan+UBSan): Open-Meteo geocode + forecast, `Environment::Service` IP location + weather + location persistence. | Success paths verified. |
| Deezer | Same probe: `GenreLookup` without Spotify credentials published Deezer genres. | Success path verified. |
| Update download | Same probe: GitHub latest release → `manual_download_and_install` downloaded the arm64 `.deb` and verified its SHA-256 (never installed). | Success path verified. Install runs on the Pi (`tests/update_transaction_test.py`). |
| Spotify (authenticated) | Not run: it needs the user's OAuth login, and the refresh token rotates, so reusing the Pi's token elsewhere could log the Pi out. | Still unverified. |
| `audio_analyzer_benchmark` in sanitizer trees | Reproduced: build-tree binaries get the install RUNPATH, which does not exist before installing, and this test (unlike the others) had no `LD_LIBRARY_PATH`. Now has a build RPATH; runs directly from `desktop_asan_build`. | Fixed. |

Full runs: emulator ASan+UBSan ctest 54/54 (the network probe skips without
`LED_MATRIX_NETWORK_TESTS=1`); `preview_gen` over all 81 previewable scenes;
desktop ASan+UBSan ctest 48/48. New finding: UBSan signed overflow in
`AudioStormFrontScene`'s noise hash (`int * 73856093`), fixed by hashing in
`uint32_t` (same bits, unchanged output).

```bash
source scripts/sanitizers/env.sh
LED_MATRIX_NETWORK_TESTS=1 ctest --test-dir emulator_asan_build -L network --output-on-failure
# LED_MATRIX_NETWORK_TESTS=no-download skips the .deb download
```

## CI and work tracking

CI runs the complete unsanitized emulator suite and selected ASan+UBSan
regressions on pushes/PRs (`build-matrix.yml`). TSan remains a local preset.
Follow-up investigations and verification work are tracked in the
[Led Matrix project](https://github.com/users/sshcrack/projects/7); the findings
and limits above retain the original audit's evidence.

## Historical build verification

Built with the new flags (non-sanitized, `-DSKIP_WEB_BUILD=ON`, -j3), zero errors:
`emulator` 313/313, `desktop-linux` 268/268, `cross-compile` (toolchain reused via
`CROSS_COMPILE_ROOT`) 197/197. `desktop-windows` was not built (no Windows host).
