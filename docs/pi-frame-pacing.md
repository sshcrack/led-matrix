# Pi frame pacing and receiver cost

Measured on `ledmat` on 2026-11-09, starting from release 1.21.4. The controller is a Raspberry Pi 3 Model B Rev 1.2, with a 128×128 display (64 rows/columns, chain 2, parallel 2), Rotate:270 and GPIO slowdown 3. The performance governor and isolated refresh CPU were already enabled. The Pi used its separate supply and reported no undervoltage/throttling at 1.2 GHz throughout the comparisons.

## Changes

`SleepMillis()` previously slept in unconditional 10 ms chunks. A renderer requesting only the remainder of a frame interval could therefore oversleep its deadline. It now waits until the earlier of the requested deadline and the next cancellation check. The skip/exit checks remain bounded by 10 ms.

The offload UDP receiver previously called `RemoteRender::status()` for every chunk just to check the active session. That diagnostic function locks state and scans the scene compatibility maps. A dedicated session check avoids constructing diagnostics; completed frames still validate their session again before publication. Compatibility checks at session creation are unchanged.

The receiver also reuses its RGB assembly storage without clearing every byte for each sequence. Exact chunk geometry and the received bitmap ensure every byte is written before publication. Invalid chunk counts are rejected, and incomplete frames never replace the latest complete frame. The wire format, protocol version 4 and 1200-byte chunk payloads are unchanged.

## Receiver benchmark on the real Pi

`remote_render_smoke --benchmark-packets` feeds the actual receiver 500 RGB888 frames at 128×128, 41 chunks/frame, with 69 matching scene contracts. Serialization is outside the measured loop. Timings include chunk handling, assembly and publication, but exclude sockets, network delivery, rendering and GPIO. The benchmark opens no matrix device, so it can run with the hardware daemon stopped.

Alternating the original and optimized plugin with the same benchmark executable and common libraries gave:

| Receiver | Three runs, ms/frame | Median ms/frame |
|---|---|---:|
| Original | 2.76843, 2.78864, 2.76877 | 2.76877 |
| Optimized | 0.040668, 0.041057, 0.040632 | 0.040668 |

This removes about 98.5% of the measured receiver time. It does not imply a proportional display-FPS gain.

## Attached-panel presentation

A temporary manual preset pinned `metablob` with a compatible desktop worker at full quality, no transition, and no required audio input. Each restart waited for a fresh remote stream before sampling `/diagnostics`. Presentation FPS is the difference in `renderer.presentation_frames` divided by monotonic elapsed time, not the instantaneous FPS field.

| Version | Sample duration | Mean presentation FPS |
|---|---:|---:|
| Original 1.21.4 | 20 s | 44.772 |
| Accurate waits + receiver changes | 20 s | 59.233 |
| Accurate waits + receiver changes, repeat | 30 s | 58.958 |

There were no reported scene errors or malformed UDP packets. Automatic Mode and the original preset were restored, and the temporary test preset was deleted afterward. The short samples establish an improvement toward 60 FPS; they do not establish uniformly spaced 60 unique frames per second for every scene/network. `copy_latest()` can present the same fresh remote sequence more than once, so presentation counts are not unique-frame counts.

## Follow-up implementation: bulk conversion and absolute deadlines

Panel library [v0.1.12](https://github.com/sshcrack/rpi-rgb-led-matrix/releases/tag/v0.1.12) provides the packed RGB888 bulk write. The app pins that release and applies no library source patches. Full-frame writes group logical pixels by their shared GPIO word, retain row-major alias ordering, cache mapper geometry with the immutable mapper, and write each active bitplane once per group. Partial rectangles keep the scalar clipping behavior. Brightness, CIE1931 mapping, RGB ordering, rotation, inverse colors, PWM depth and untouched GPIO bits retain their original semantics. Desktop offload, video, shader streams and AudioKit float buffers use the bulk path. The emulator retains its software-pixel implementation.

Preview behavior is unchanged: automatic requests while visible, no capture without outstanding demand. Hardware readback now lazily caches the exact inverse-gamma lookup on its first requested read. It neither allocates a preview RGB frame nor reads pixels when nobody requests a capture. Tables are immutable and initialized safely for concurrent readers.

A reused desktop transition session now receives an optional `target_fps` state field when its requested rate changes. The worker updates the deadline and scene rate without recreating the scene, resetting sequence numbers or changing elapsed animation time. Reconnect commands retain the new rate. This is additive within protocol 4: older workers ignore the field safely, but need an update to gain the bandwidth saving.

The Pi renderer uses absolute microsecond deadlines with fractional periods (60 FPS alternates 16,666/16,667 microsecond increments), fills only the remainder left by VSync, and skips missed deadline slots after stalls instead of bursting. Long waits remain cancellable in at most 10 ms.

An isolated GPIO-free benchmark on the real Pi alternated the installed 1.21.5 library and candidate three times, 180 warmed iterations per mode. The daemon was stopped during timing and restarted afterward. CPU clock was 1.2 GHz and throttling flags remained `0x0`. This measures conversion cost, excluding GPIO scanout, sockets, scene rendering and presentation.

| Operation | Installed median ms/frame | Candidate median ms/frame |
|---|---:|---:|
| Scalar full-frame write | 7.226 | 7.218 |
| `SetPixels` full-frame write | 6.295 | 4.428 |
| Full `GetPixel` readback | 11.760 | 7.020 |

Moving production RGB blits from the scalar path to bulk saves about 2.80 ms per frame (38.7%); the existing `SetPixels` API also improves by 29.6%. Readback saves about 4.74 ms per requested capture (40.3%). These figures do not imply the same percentage improvement in scene FPS. Raw logs are local artifacts under `output/performance/bulk-cadence/`.

The library-owned `bulk-framebuffer-test` compares complete serialized GPIO bitplanes against scalar writes across three row counts, three parallel layouts, rotations/mirrors, RGB/BGR, inverse colors, gamma, four brightness settings and three PWM depths. It covers clipped rectangles, mapper changes after cache warm-up, aliased pixels and untouched planes. Readback matches the unmodified library fingerprint `1623568448239114217`. Both the old and patched libraries were run against this corpus on the laptop, and the patched library passed on the Pi. `frame_cadence_smoke` checks fractional phase retention, jitter and stall recovery; the renderer integration test failed at 97,000 µs before the fix and passes at the expected 100,334 µs afterward. The remote regression likewise failed before the rate update.

### Full daemon trial on the attached panels

The same `metablob` preset was measured at full desktop quality with and without a localhost preview viewer, waiting 75 ms after each received capture as the web UI does. The installed daemon was sampled twice around one candidate trial (A/B/A), about 30.6 seconds per condition. All retained samples reported desktop placement and quality 1.0, with no viewer errors or throttling. The preview delivered about 10–11 captures/s. Baseline CPU below is the mean of its two trials; CPU percentages use one core as 100%.

| Condition | Installed FPS range | Candidate FPS | Installed CPU | Candidate CPU |
|---|---:|---:|---:|---:|
| No preview requests | 58.66–58.72 | 59.81 | 137.8% | 127.1% |
| Automatic preview visible | 53.26–53.36 | 59.96 | 148.1% | 136.7% |

The candidate increased presentation FPS by about 1.9% without preview and 12.5% with preview while reducing daemon CPU by roughly 7.7%. These counters include repeated fresh remote sequences and do not establish 60 unique, evenly spaced frames every second. Worker sequence rates were about 59.4–60/s. Loopback preview excludes the added cost of serving the browser across Wi-Fi.

Two preliminary candidate trials were excluded: the first started before every plugin finished uploading and fell back to local reduced-quality rendering; another had a transient placement change. The retained trial verified all 18 plugins loaded before measurement and checked full-quality desktop placement at every sample. The original service, preset and Automatic Mode were restored after each trial. This was a temporary candidate run, not a package deployment.

The full emulator build passed 52 tests; the full Linux desktop build passed 49 tests, including worker state continuity and saved contract fixtures. The ARM build also completed. Raw retained samples, discarded-run logs and conversion timings remain local under `output/performance/bulk-cadence/`.

The library implementation and its bitplane regression test now live in `sshcrack/rpi-rgb-led-matrix` (PR #11, release v0.1.12), including the Windows compatibility change previously patched by this app. Its hardware and SDL CMake configurations passed 5 and 8 tests respectively, and Linux/Python/Windows CI passed before release. The app retains the cadence, transport, worker and integration regressions. After switching to the published release, its emulator and Linux desktop builds passed 51 and 48 tests respectively, and the ARM build succeeded.

## Further opportunities

- Local scenes that draw pixels individually still pay scalar conversion costs. Wider software RGB composition could let more scenes and effects use one final bulk conversion.
- Hardware readback still costs roughly 7 ms per capture. Keep live capture demand-driven, and prefer software RGB composition plus one final conversion when optimizing local transitions/effects.
- Report unique produced/received/presented sequences and per-stage timing before claiming locked 60 FPS. Lower PWM is a visual-quality tradeoff and is not required for the gains above.

## Regression coverage

`scene_renderer_cadence_smoke` exercises the real short waiter: requesting 3 ms previously produced a median 10.06 ms wait, while the fix passes with scheduling tolerance. It also checks long-wait cancellation and the renderer's VSync pacing. `remote_render_smoke` covers session reuse/replacement/stop, reordered/duplicate/partial frames, and invalid chunk counts; its cross-compile build skips emulator canvas creation. The handoff, held-frame, runtime selection and remote contract smoke tests also pass.

Build through the presets. For the Pi receiver-only check, configure `cross-compile` with `-DSKIP_WEB_BUILD=ON`, build target `remote_render_smoke`, and run it on the Pi with `--benchmark-packets` and the matching candidate libraries. It does not need root or GPIO output.
