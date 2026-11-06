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

## Desktop composition

The worker now receives a single RGB888 outgoing snapshot in its scene-start command, renders the incoming scene, applies the registered transition on its headless software canvases, and streams completed frames. The Pi performs only the final framebuffer write and presentation. After the transition, ordinary scene rendering reuses the session. Protocol version 3 requires updating both Pi and desktop; older workers are not eligible. The existing scene-code fingerprint must also match.

Startup without a frame times out after 500 ms and falls back to local composition. A stream stalled for 150 ms after its first frame hands control to the incoming scene without replaying the transition. Reconnect commands resume the scene without replaying the old outgoing snapshot. Held incoming scenes preserve the snapshot until they produce real pixels. Postprocessing and demand-driven live capture remain on the Pi.

Validation includes worker self-tests for every registered transition and its completion, transport tests for session reuse and reconnect, and matrix-side tests for receiving already-composed pixels, worker stalls, and startup timeout.

### Attached-panel end-to-end measurements

Measured on the same Pi and attached 128×128 panel with GPIO slowdown 3, alternating `metablob` and `neontunnel`, ten-second scene durations and five-second blend transitions. Unlike the fixed-gradient measurements above, these include actual scene rendering and, for desktop placement, the WebSocket start/snapshot, UDP transport and Pi GPIO presentation. Runs used the same candidate code with a compatible desktop worker connected versus no worker. A parent desktop control connection was present for the worker, as required by normal producer election.

| Placement | Direction | Presented frames | FPS | Startup to first streamed frame |
|---|---|---:|---:|---:|
| local | neontunnel → metablob | 126 | 25.16 | — |
| local | metablob → neontunnel | 109 | 21.76 | — |
| desktop | metablob → neontunnel | 295 | 59.76 | 91 ms |
| desktop | neontunnel → metablob | 295 | 59.70 | 81 ms |
| desktop | metablob → neontunnel | 294 | 59.64 | 91 ms |
| desktop | neontunnel → metablob | 290 | 58.94 | 101 ms |

Desktop mean: 59.51 FPS. Local mean: 23.46 FPS. These are short, controlled samples, not a guarantee for all scenes or networks. FPS covers first-to-last transition presentation; startup is reported separately and holds the outgoing image. All registered transition types pass worker composition/handoff self-tests; this end-to-end timing run covers blend only. The original Pi service and config were restored after each run.
