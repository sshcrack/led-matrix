# Transition performance on the attached Pi

Historical measurements from 2026-11-06 on `ledmat`, comparing bcc108b7 against its parent. These timings predate the bulk-write and cadence improvements in [Pi frame pacing](pi-frame-pacing.md); they are not current performance baselines.

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

The worker now receives a single RGB888 outgoing snapshot in its scene-start command, renders the incoming scene, applies the registered transition on its headless software canvases, and streams completed frames. The Pi performs only the final framebuffer write and presentation. After the transition, ordinary scene rendering reuses the session. This feature originally shipped with protocol 3 and source fingerprints. Current eligibility uses protocol 4 and per-scene data-contract versions; see [Remote data contracts](SCENE_EXECUTION.md#remote-data-contracts).

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

## Transition start freeze (2026-11-13)

Measured on `ledmat` (Pi 3B, 128×128, release 1.22.0 baseline) with the desktop worker on a laptop; both on Wi-Fi. Each trial ran a temporary manual preset cycling `metablob`, `neontunnel`, `boids` and `starfield` (8 s scenes, 1 s blend, ~27 transitions per trial). The candidate ran as a transient unit with the service's flags and environment, the service was restarted afterwards, and the test preset was deleted.

### Per-transition diagnostics

`/diagnostics` → `transition` now carries frame pacing for the last transition, and `transition_history` keeps the last 32:

| Field | Meaning |
|---|---|
| `prepare_ms` | Outgoing scene stopped → transition composing begins (director choice, scene initialisation; after any beat-synchronised hold) |
| `setup_ms` | Composing begins → first transition frame presented |
| `lead_in_gap_ms` | Last outgoing presentation → first transition frame: how long the picture stood still |
| `request_ms` | Desktop transitions: render-thread time to snapshot and send the start command |
| `interval_ms_p50/p95/max`, `late_frames` (>25 ms), `stalls` (>50 ms) | Presentation intervals inside the transition |
| `handoff_gap_ms` | Last transition frame → first frame of the scene phase |

### Findings

In Automatic Mode on 1.22.0, desktop-composed transitions froze the outgoing frame for 130–450 ms before the first transition frame. Transitions themselves last about 0.6 s. The controlled baseline gave a 176 ms median (max 291 ms), with `request_ms` at 57 ms. Building, copying and serialising the outgoing snapshot as a 49,152-number JSON array dominated that render-thread cost. Worker scene start is about 1 ms. The rest is WebSocket delivery of the start command and the first UDP frame back.

The start command now sends the snapshot as `from_base64` when the worker lists `transition_snapshot_base64` in its heartbeat `features`. This is additive within protocol 4: older workers keep getting `from`, and new workers accept both. The transition loop also reads the newest frame sequence without `RemoteRender::status()`, which locked the transport and scanned the contract maps on every 1 ms poll.

Same worker build and LAN path, trials interleaved:

| Pi build | Trials | `request_ms` median | Freeze median | Freeze max |
|---|---|---:|---:|---:|
| Baseline (array snapshot) | 4 | 53.5–65.7 | 147–170 | 198–309 |
| Base64 snapshot | 6 | 16.0–16.6 | 101–112 | 124–292 |

The median freeze drops by about 50 ms (~3 frames at 60 FPS). In-transition presentation stays at ~60 FPS with a 12 ms median interval. Late frames (>25 ms) varied between 0.37 and 1.46 per transition across the evening in both arms; the last interleaved pair gave 0.93 (base64) vs 1.04 (baseline), and these gaps were spread evenly through the transitions. A raw UDP test (60 FPS × 41 chunks) showed 0.3–3.5% incomplete frames and a p95 frame-arrival interval of ~25 ms on this Wi-Fi, so these gaps are network jitter rather than Pi work.

Two further observations:

- Through WireGuard (desktop host set to the Pi's `10.6.0.x` address), traffic takes two Wi-Fi hops via the VPN hub. That added about 30 ms to every transition start compared with the LAN address (baseline: 176 ms vs 147 ms median). Use the matrix's LAN address on the home network.
- Presentation intervals alternate ~11 ms and ~22 ms: `SwapOnVSync` on this panel runs at about 90 Hz while desktop frames arrive at 60 FPS, so each frame is shown for one or two refreshes.

### Frame references, ordered command sending and transition clock

When the outgoing scene is itself rendered on the desktop, the Pi no longer sends its pixels back. `DesktopHandoff::copy_scene_frame()` remembers the `{session, sequence}` of the frame it copied. If the worker lists `transition_from_frame` in its heartbeat `features`, the start command carries `transition.from_frame` instead of a snapshot. The worker keeps its last 16 sent frames, takes the referenced one (or the closest one of that session) before resetting, and refuses the start if that session's frames are gone (the Pi then falls back after the 500 ms startup timeout). A local outgoing scene, or a worker without the feature, still gets `from_base64` or `from`.

`RemoteRender` now queues start, state and stop commands and the state publisher thread serialises and sends them in order, so the render thread no longer waits on JSON or the socket. A stored start command never holds a transition, so a reconnect resumes the scene instead of replaying the handoff.

The Pi now counts a desktop transition's duration from its first composed frame, not from the request. The worker starts its blend when the command arrives, so with a 100 ms start the Pi used to switch to the scene phase (and possibly change the target FPS) mid-blend.

A locally composed transition that follows a failed desktop start reports `desktop_fallback: true`.

Same evening, LAN path, worker built from the same source, `metablob → neontunnel → boids → starfield` (all desktop-rendered):

| Pi build | Transitions | `request_ms` median | Freeze median | Freeze max |
|---|---:|---:|---:|---:|
| Base64 snapshot (earlier trial) | 27 | 16.0 | 112 | 155 |
| Frame reference + queued sending | 13 | 1.0 | 56 | 77 |

In-transition pacing is unchanged (p95 interval ~23 ms, about 2 late frames per transition).

### Frame reassembly on the Pi

The UDP receiver keeps three frames in assembly, so chunks of frame N that arrive after the first chunks of N+1 still complete N. `/diagnostics` → `udp.remote_frames` counts `completed`, `incomplete` (dropped because a newer frame completed or the slots ran out), `reorder_recovered` (completed while a newer frame was pending), `late_chunks` and `duplicate_chunks`. On `ledmat`, one evening's counters were 8,015 completed, 60 incomplete (0.7%) and 0 reordered, late or duplicate frames. A second run was 6,018 completed and 185 incomplete (3%) while the laptop worker also served the desktop app. Losses are dropped datagrams, not reordering. A frame is only known to be incomplete once a newer one has completed, so showing partial frames would not help.

### Panel refresh vs 60 FPS frames

`demo -D 4 --led-show-refresh` on `ledmat` with the service's flags (service stopped):

| Extra flags | Refresh (median) |
|---|---:|
| none | 97 Hz |
| `--led-pwm-bits=10` | 99 Hz |
| `--led-pwm-bits=9` | 104 Hz |
| `--led-pwm-lsb-nanoseconds=100` | 117 Hz |
| `--led-pwm-dither-bits=1` | 157 Hz |

Under the daemon, refresh is about 90 Hz, so a 60 FPS desktop frame is shown for one or two refreshes (11/22 ms). With `--led-limit-refresh=120 --led-pwm-dither-bits=1`, the median interval became an even 16.5 ms. The p95 interval (25 vs 23 ms) and late frames per transition (4 vs 2) did not improve, because network jitter dominates them. The default panel flags therefore stay unchanged. Dithering would also change the low-brightness look.

A worker capped at 45 FPS (two refreshes per frame) gave an even 22 ms median interval. But each lost frame then left a 44 ms gap: p95 was 33 ms vs 23 ms at 60 FPS, with 7.5 vs 2 late frames and 1 vs 0 stalls per transition (about 10 transitions each, same evening). Desktop frames therefore stay at 60 FPS.

### Audio relayed to the worker

`state` commands relay the Pi's audio frame with `age_seconds`. The worker ages the frame from when the Pi received it, applies each `sequence` only once, and ignores frames the Pi already considers stale. Before this, every 40 ms state message re-stamped the last frame as fresh, so the worker kept audio-reactive scenes "live" after the music stopped.
