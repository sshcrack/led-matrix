# Transition performance on the attached Pi

Measured 2026-11-06 on `ledmat`, comparing bcc108b7 against its parent.

Both executables used the actual installed `/usr/lib/led-matrix/librpi-rgb-led-matrix.so`, GPIO output and the attached 128×128 panel (64 rows/columns, chain 2, parallel 2, GPIO slowdown 3). Each called the source transition implementation on two fixed gradient frames, followed by `SwapOnVSync(..., 1)`. Each run rendered 24 frames at twelve progress values from 0.04 through 0.92, repeated twice. Execution order: old, new, new, old. Values below average the two runs per version (48 presented frames per effect/version).

The normal daemon was stopped for the test and successfully restarted afterward. These are real panel presentation measurements of the compositor with fixed source frames, **not whole-scene transition FPS**: incoming scene rendering, postprocessing and remote-worker handoff are excluded. FPS is 1000 / mean frame interval. No uncertainty estimate is claimed from these short samples.

| Transition | Before ms/frame | After ms/frame | Before FPS | After FPS | Frame-time reduction |
|---|---:|---:|---:|---:|---:|
| Blend | 65.34 | 66.91 | 15.3 | 14.9 | -2.4% |
| Swipe | 66.88 | 40.45 | 15.0 | 24.7 | 39.5% |
| Morph | 67.10 | 66.15 | 14.9 | 15.1 | 1.4% |
| RadialReveal | 67.02 | 44.23 | 14.9 | 22.6 | 34.0% |
| CheckerReveal | 67.08 | 53.79 | 14.9 | 18.6 | 19.8% |
| OrderedDissolve | 66.98 | 40.68 | 14.9 | 24.6 | 39.3% |
| RandomDissolve | 66.96 | 41.12 | 14.9 | 24.3 | 38.6% |
| ZoomBlend | 66.95 | 66.94 | 14.9 | 14.9 | 0.0% |
| Glitch | 53.51 | 53.54 | 18.7 | 18.7 | -0.0% |
| CrtCollapse | 23.23 | 23.25 | 43.1 | 43.0 | -0.1% |
| BlockDissolve | 66.73 | 40.57 | 15.0 | 24.6 | 39.2% |

Reveal effects improve by about 20–40% in frame time. Blend and morph have no meaningful improvement; unchanged zoom/glitch/CRT effects serve as controls. The remaining ~40 ms/frame cost on reveal effects means this fix alone does not reach 60 FPS. Further work is needed to avoid hardware framebuffer readback during composition.
