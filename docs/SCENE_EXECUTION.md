# Scene execution, previews, and desktop offload

A scene is implemented once and can be executed in three environments:

1. **Matrix** — the Raspberry Pi renders the scene directly.
2. **Preview** — `preview_gen` renders the same scene against a headless/emulated canvas.
3. **Remote worker** — `led-matrix-scene-worker` on the desktop renders the same scene implementation and streams RGB frames back to the Pi.

The goal is that scene authors do **not** write a Pi renderer and a separate desktop renderer.

## Source layout

Scene code belongs directly under the plugin's `scenes/` directory:

```text
plugins/MyPlugin/
├── CMakeLists.txt
├── scenes/
│   ├── MyScene.cpp
│   └── MyScene.h
├── matrix/
│   ├── MyPlugin.cpp
│   └── MyPlugin.h
└── desktop/                 # optional; UI/input/control only
    ├── MyDesktopPlugin.cpp
    └── MyDesktopPlugin.h
```

`matrix/` contains the matrix-side plugin integration. `desktop/` is only needed when the plugin has actual desktop-specific behavior such as an ImGui UI, audio capture, or another desktop data source. It is **not** where an alternate copy of a scene renderer belongs.

## Automatic remote execution

`SceneCapabilities::supports_remote_rendering` defaults to `true`.

That means a normal portable scene is automatically included in the desktop scene worker. At runtime the Pi still starts by rendering locally. The renderer profiler measures the real cost on the actual Pi and can ask the desktop worker to take over when sustained rendering pressure is too high.

The handoff is deliberately conservative:

- the Pi continues rendering while the worker starts;
- the worker advertises the exact scene IDs it can instantiate;
- the Pi only requests offload for an advertised scene;
- the Pi switches once a complete fresh remote frame exists;
- frames are latest-wins rather than queued, so network jitter cannot accumulate latency;
- if the worker disappears or a frame becomes stale, the Pi immediately falls back to local rendering;
- post-processing and final matrix presentation remain on the Pi.

This placement decision is internal. Users do not need to maintain separate scene configurations for local and remote rendering.

### Opting out

Only opt out when the scene fundamentally cannot execute in the worker process, for example because its renderer talks directly to matrix-only hardware or relies on process-local state that cannot be represented by Runtime Inputs/state migration.

```cpp
Scenes::SceneCapabilities MyScene::get_capabilities() const {
    auto caps = Scene::get_capabilities();
    caps.supports_remote_rendering = false;
    return caps;
}
```

Do not opt out merely because the scene is expensive. Expensive portable scenes are exactly what the worker is for.

Current intentional opt-outs include lightweight proxy scenes such as `Video`, `Shadertoy`, and `SpotifyMV`: their expensive decoding/GPU/video work already happens in a dedicated desktop plugin and the Pi scene only displays those streamed pixels. Sending that proxy through the scene worker would add another network hop without moving meaningful compute.

## Runtime Inputs

External data should enter scenes through Runtime Inputs instead of through hidden process-local globals whenever practical.

A scene declares what it needs:

```cpp
Scenes::SceneInputSpec MyScene::get_runtime_input_spec() const {
    Scenes::SceneInputSpec spec;
    spec.require("my.input");
    spec.accept(RuntimeInputIds::Audio);
    return spec;
}
```

The producing plugin declares the IDs it owns:

```cpp
std::vector<std::string> MyPlugin::get_runtime_input_ids() const {
    return {"my.input"};
}
```

and publishes state:

```cpp
RuntimeInputs::publish(
    "my.input",
    {{"value", 42.0}, {"label", std::string("example")}},
    std::chrono::seconds(2));
```

The complete Runtime Input snapshot is mirrored into a remote worker session. Audio is mirrored as the full `AudioProtocol::Frame`, including features, spectrum, waveform, event flags, and counters.

Spotify is an example of a network-backed scene whose render path can consume mirrored runtime state (track, artist, progress, duration, artwork URL) rather than requiring the worker to own the Pi-side Spotify polling thread.

## Scene state during migration

Properties, variant ID, scene UUID, target FPS, and elapsed scene time are transferred automatically.

Most deterministic/procedural scenes therefore need no additional work. A stateful scene can optionally transfer transient simulation state:

```cpp
nlohmann::json MyScene::snapshot_runtime_state() const {
    return {
        {"phase", phase_},
        {"particles", particles_}
    };
}

void MyScene::restore_runtime_state(const nlohmann::json& state) {
    phase_ = state.value("phase", phase_);
    if (state.contains("particles"))
        particles_ = state.at("particles").get<std::vector<Particle>>();
}
```

Keep this payload compact. It is only for state that is necessary to avoid a visible reset during a placement change.

## Timing

The canonical render entry point is `Scene::render_frame()`. Scene implementations override only:

```cpp
bool render(rgb_matrix::FrameCanvas* canvas) override;
```

For animation time, prefer the scene frame context:

```cpp
const auto& frame = frame_context();
const double dt = frame.delta_seconds;
const double t = frame.elapsed_seconds;
```

This keeps matrix rendering, previews, and worker rendering consistent. Avoid reading wall-clock time inside a renderer unless the scene truly represents wall time (for example a clock).

`SwapOnVSync`/the renderer coordinator owns frame pacing. Do not add an unconditional sleep in a scene render loop. `wait_until_next_frame()` remains supported for older code, but `render_frame(..., suppress_internal_wait=true)` is used by the central renderer and worker so pacing does not happen twice.

If a scene stays active but deliberately has no new pixels to present (for example while an async image is loading or media is paused), call `hold_current_frame()` before returning from `render()`. The local renderer then leaves the currently displayed buffer latched instead of swapping an untouched historical double buffer. The scene is still polled at its normal target rate so it can resume immediately when state changes. Transition delays honor the same contract, and an incoming held scene keeps the outgoing snapshot until it produces a real frame, so reusable transition canvases cannot leak stale pixels.

For fixed-rate simulations use `Scenes::FixedStepAccumulator` rather than coupling physics updates to render FPS.

## Adaptive local quality

`Scene::report_render_cost()` maintains a render quality scale based on measured frame cost. Expensive scenes can read:

```cpp
const float quality = render_quality_scale();
```

and reduce optional detail while the Pi is under pressure. A scene may start conservatively:

```cpp
set_render_quality_hint(0.8f);
```

Remote offload and adaptive local quality complement each other: local quality protects the Pi immediately, while the worker can take over sustained expensive workloads.

## Desktop scene worker

Linux and Windows desktop builds include `led-matrix-scene-worker` by default (`ENABLE_SCENE_WORKER=ON`). The process is supervised by the desktop `RenderOffload` plugin. On Windows the worker uses the native headless backend from our `rpi-rgb-led-matrix` fork, so it does not depend on Raspberry Pi GPIO, POSIX threading/mmap APIs, or SDL.

The worker:

- loads worker copies of the normal matrix plugin DSOs;
- runs in `SceneExecution::Mode::RemoteWorker`;
- uses the headless matrix emulator as the scene canvas;
- reports a heartbeat containing its available scene IDs;
- receives start/stop/state commands over the existing matrix WebSocket;
- sends RGB888 frames back using MTU-sized UDP chunks;
- is counted separately from the normal desktop GUI connection.

The worker and desktop GUI are separate processes intentionally. `SharedToolsMatrix` and `SharedToolsDesktop` each have their own plugin-manager ABI/namespaces and should not be linked into one process.

### Useful worker check

From a desktop build tree:

```bash
./desktop_build/led-matrix-scene-worker \
  --list-scenes \
  --plugin-dir ./desktop_build/scene_plugins
```

`--list-scenes` writes one JSON array to stdout, making it suitable for tests and diagnostics.

## Preview execution

Preview generation is another execution mode, not a separate renderer implementation. A portable scene is previewable by default.

Scenes that need external data declare preview fixture inputs:

```cpp
Previews::SceneSpec MyScene::get_preview_spec() const {
    return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
}
```

A plugin supplies a `Previews::DataProvider` for that input. The provider should update the same runtime state that production uses; the scene itself should not contain preview-only rendering branches.

## Resource files

A scene may still load resources relative to `get_plugin_location()`. Resource install rules must cover both matrix plugins and worker plugins. Countdown and WeatherOverview, for example, install/copy their BDF fonts beside both forms of the plugin.

When adding a new resource-backed scene, verify both:

```text
lib/led-matrix/plugins/<Plugin>/...                 # matrix package
lib/led-matrix-desktop/scene-plugins/<Plugin>/...   # desktop worker package
```

## Diagnostics

The matrix Diagnostics page/API exposes the measured scene render percentiles and current placement. The important fields are:

- scene render p50/p95/p99;
- target frame budget and p95/budget ratio;
- adaptive quality scale;
- placement (`local`, `desktop_pending`, `desktop`, or local fallback);
- worker availability and advertised scene count;
- remote session/sequence/frame age.

Use these real Pi measurements when deciding what to optimize. Desktop/QEMU timings are useful for regressions but are not an accurate model of absolute Raspberry Pi performance.
