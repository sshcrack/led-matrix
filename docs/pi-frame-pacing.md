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

## Remaining opportunities

- The renderer still uses integer 16 ms periods for a declared 60 FPS and anchors pacing to actual starts. An absolute fractional deadline can reduce accumulated scheduling drift. An isolated panel harness reached 59.93 FPS with absolute 60 Hz pacing, but that is not a whole-scene result.
- RGB-to-panel conversion remains significant: a full SetPixel pass took about 7.37 ms at healthy Pi power. Simply using SetPixels did not materially change this because its implementation also loops over SetPixel.
- A full hardware GetPixel readback added roughly 11 ms in the same harness. Keep live capture demand-driven, and prefer software RGB composition plus one final conversion when optimizing local transitions/effects.
- Report unique produced/received/presented sequences and per-stage timing before claiming locked 60 FPS. Lower PWM is a visual-quality tradeoff and is not required for the gains above.

## Regression coverage

`scene_renderer_cadence_smoke` exercises the real short waiter: requesting 3 ms previously produced a median 10.06 ms wait, while the fix passes with scheduling tolerance. It also checks long-wait cancellation and the renderer's VSync pacing. `remote_render_smoke` covers session reuse/replacement/stop, reordered/duplicate/partial frames, and invalid chunk counts; its cross-compile build skips emulator canvas creation. The handoff, held-frame, runtime selection and remote contract smoke tests also pass.

Build through the presets. For the Pi receiver-only check, configure `cross-compile` with `-DSKIP_WEB_BUILD=ON`, build target `remote_render_smoke`, and run it on the Pi with `--benchmark-packets` and the matching candidate libraries. It does not need root or GPIO output.
