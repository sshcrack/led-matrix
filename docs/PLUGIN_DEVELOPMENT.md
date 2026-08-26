# Plugin development guide

Plugins can contribute scenes, Runtime Inputs, image/shader providers, post-processing effects, transitions, REST/WebSocket handlers, and optional desktop UI/input logic.

The most important architectural rule is:

> **Write a scene renderer once, under `plugins/<Plugin>/scenes/`.**
>
> The same scene implementation is used on the Raspberry Pi, by `preview_gen`, and by the desktop scene worker for automatic render offload.

See [Scene execution, previews, and desktop offload](SCENE_EXECUTION.md) for the execution model in detail.

## Plugin layout

```text
plugins/MyPlugin/
├── CMakeLists.txt
├── scenes/
│   ├── MyScene.cpp
│   └── MyScene.h
├── matrix/
│   ├── MyPlugin.cpp
│   └── MyPlugin.h
└── desktop/                    # optional
    ├── MyDesktopPlugin.cpp
    └── MyDesktopPlugin.h
```

Use the directories as follows:

- `scenes/`: portable scene implementations. Do not put these below `matrix/` or duplicate them below `desktop/`.
- `matrix/`: plugin registration, matrix-side data producers, REST routes, UDP/WebSocket handlers, providers, and other Pi integration.
- `desktop/`: only desktop-specific UI/input/control code such as ImGui panels, audio capture, or a desktop-only data producer.

Linux and Windows desktop builds automatically compile worker copies of matrix plugin sources when `ENABLE_SCENE_WORKER=ON` (the default). New portable scenes therefore become eligible for remote execution without a second renderer implementation. Windows uses the fork's native headless matrix backend; no Raspberry Pi GPIO or POSIX emulator shim is required.

## Minimal scene plugin

### `scenes/MyScene.h`

```cpp
#pragma once

#include <shared/matrix/Scene.h>
#include <shared/matrix/wrappers.h>

namespace Scenes {

class MyScene final : public Scene {
public:
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    std::string get_name() const override { return "my_scene"; }
    std::string get_category() const override { return "Examples"; }
    void register_properties() override;

protected:
    tmillis_t get_default_duration() override { return 15000; }
    int get_default_weight() override { return 10; }

private:
    PropertyPointer<float> speed =
        MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.05f, 5.0f);
};

class MySceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override;
};

} // namespace Scenes
```

### `scenes/MyScene.cpp`

```cpp
#include "MyScene.h"

#include <algorithm>
#include <cmath>

using namespace Scenes;

bool MyScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const double t = frame_context().elapsed_seconds * speed->get();
    const auto level = static_cast<std::uint8_t>(
        std::clamp((std::sin(t) * 0.5 + 0.5) * 255.0, 0.0, 255.0));

    for (int y = 0; y < matrix_height; ++y)
        for (int x = 0; x < matrix_width; ++x)
            canvas->SetPixel(x, y, level, 32, 255 - level);

    return true;
}

void MyScene::register_properties()
{
    add_property(speed);
}

std::unique_ptr<Scenes::Scene> MySceneWrapper::create()
{
    return std::make_unique<MyScene>();
}
```

Prefer `frame_context().delta_seconds` and `frame_context().elapsed_seconds` over reading `steady_clock` directly. This keeps matrix, preview, and remote-worker animation timing consistent.

## Matrix plugin class

`BasicPlugin` owns the integration points. A small scene-only plugin can look like this:

```cpp
#pragma once

#include <shared/matrix/plugin/main.h>

class MyPlugin final : public Plugins::BasicPlugin {
public:
    std::string get_plugin_name() const override { return PLUGIN_NAME; }

private:
    std::vector<std::unique_ptr<Plugins::SceneWrapper>> create_scenes() override;
    std::vector<std::unique_ptr<Plugins::ImageProviderWrapper>> create_image_providers() override {
        return {};
    }
};
```

```cpp
#include "MyPlugin.h"
#include "scenes/MyScene.h"

REGISTER_PLUGIN(MyPlugin, MyPlugin)

std::vector<std::unique_ptr<Plugins::SceneWrapper>> MyPlugin::create_scenes()
{
    std::vector<std::unique_ptr<Plugins::SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<Scenes::MySceneWrapper>());
    return scenes;
}
```

Use `REGISTER_PLUGIN`/`REGISTER_PLUGIN_CUSTOM_DESTROY`; do not hand-write obsolete `createFoo`/`destroyFoo` exports.

## CMake registration

A scene-only plugin:

```cmake
register_plugin(MyPlugin
    matrix/MyPlugin.cpp
    matrix/MyPlugin.h
    scenes/MyScene.cpp
    scenes/MyScene.h
)
```

A plugin with optional desktop UI/input code:

```cmake
register_plugin(MyPlugin
    matrix/MyPlugin.cpp
    matrix/MyPlugin.h
    scenes/MyScene.cpp
    scenes/MyScene.h
    DESKTOP
    desktop/MyDesktopPlugin.cpp
    desktop/MyDesktopPlugin.h
)
```

Everything before `DESKTOP` is the matrix/scene side and is also compiled into the worker copy for desktop builds. Everything after `DESKTOP` belongs only to the normal desktop plugin.

When a worker copy needs an extra dependency, link its generated `<Plugin>SceneWorker` target:

```cmake
if(TARGET MyPluginSceneWorker)
    target_link_libraries(MyPluginSceneWorker PRIVATE my_dependency)
endif()
```

If a scene loads resource files relative to its plugin DSO, install/copy those resources for both matrix and worker layouts. See `Countdown` and `WeatherOverview` for examples.

## Properties

Properties are serialized automatically and travel with a scene when it is offloaded.

```cpp
PropertyPointer<int> count = MAKE_PROPERTY("count", int, 10);
PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 1.0f);
PropertyPointer<bool> enabled = MAKE_PROPERTY("enabled", bool, true);
PropertyPointer<std::string> label = MAKE_PROPERTY("label", std::string, "hello");

PropertyPointer<int> brightness =
    MAKE_PROPERTY_MINMAX("brightness", int, 50, 0, 100);
```

Register each property once:

```cpp
void MyScene::register_properties()
{
    add_property(count);
    add_property(speed);
    add_property(enabled);
    add_property(label);
    add_property(brightness);
}
```

Do not maintain a separate property/config format for the desktop worker. `Scene::to_json()` is the canonical scene-property payload.

## Scene metadata and variants

`SceneDescriptor` is consumed by automatic selection and the web UI. Override `get_descriptor()` when useful:

```cpp
Scenes::SceneDescriptor MyScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "ambient";
    d.tags = {"procedural", "calm"};
    d.intensity = 0.35f;
    d.motion = 0.45f;
    d.performance_cost = 0.55f;
    return d;
}
```

`performance_cost` is a scheduling hint, not a hardcoded offload flag. The Pi profiler measures actual runtime cost and decides placement from real device timings.

Variants can provide named property bundles through the descriptor. The same variant ID and resulting properties are transferred to the worker.

## Runtime Inputs

Use Runtime Inputs for external state that a scene consumes. This avoids coupling render code to the process that originally acquired the data and makes the same scene usable in previews and remote execution.

A producer declares the IDs it owns:

```cpp
std::vector<std::string> MyPlugin::get_runtime_input_ids() const override
{
    return {"my.input"};
}
```

Publish data:

```cpp
RuntimeInputs::publish(
    "my.input",
    {
        {"temperature", 21.5},
        {"label", std::string("office")},
        {"active", true}
    },
    std::chrono::seconds(5));
```

A scene declares requirements:

```cpp
Scenes::SceneInputSpec MyScene::get_runtime_input_spec() const override
{
    Scenes::SceneInputSpec spec;
    spec.require("my.input");
    spec.accept(RuntimeInputIds::Audio);
    return spec;
}
```

and reads a snapshot:

```cpp
const auto inputs = RuntimeInputs::snapshot();
const double temp = inputs.number("my.input", "temperature").value_or(0.0);
```

Required inputs participate in scene eligibility. If a required input disappears while a scene is running, the scheduler can replace that scene.

An automatically eligible scene may optionally override `prepare_runtime(const RuntimeInputs::Snapshot&)` to start asynchronous preparation before it is selected. Keep this hook non-blocking and idempotent; it runs from the Automatic Director path while a different scene may be rendering. Use it to enqueue/request work, not to fetch or decode synchronously. SpotifyMV uses this to prepare the current track on the desktop without sending its high-bandwidth frame stream until SpotifyMV is actually active.

The worker receives mirrored Runtime Inputs automatically. Do not create a worker-specific socket or duplicated data model unless the data fundamentally cannot be represented as Runtime Inputs.

## Audio-reactive scenes

Audio state is available through `AudioState::snapshot()` and is mirrored to the remote worker as the full rich audio frame. Scenes may declare audio capability in `get_capabilities()`:

```cpp
Scenes::SceneCapabilities MyScene::get_capabilities() const override
{
    auto caps = Scene::get_capabilities();
    caps.supports_audio = true;
    return caps;
}
```

Use `requires_audio = true` only when the scene is unusable without audio. Use `supports_audio = true` when audio merely enhances it.

## Automatic desktop offload

Portable scenes are offloadable by default:

```cpp
SceneCapabilities::supports_remote_rendering == true
```

You normally do nothing. The desktop build packages a `led-matrix-scene-worker` process containing worker copies of the same matrix plugins. The Pi asks it to render only when the real Pi measurements show sustained pressure and the worker says that scene is available.

Opt out only for a genuinely non-portable renderer:

```cpp
Scenes::SceneCapabilities MyHardwareScene::get_capabilities() const override
{
    auto caps = Scene::get_capabilities();
    caps.supports_remote_rendering = false;
    return caps;
}
```

Do **not** create `MySceneDesktopRenderer`. The worker exists specifically to remove that duplication.

A useful exception is a scene that is already only a thin proxy for desktop-rendered pixels. `Video`, `Shadertoy`, and `SpotifyMV` intentionally set `supports_remote_rendering = false`: their expensive work is already performed by a dedicated desktop plugin, so worker offload would add a redundant second hop.

### Migrating transient simulation state

The worker automatically receives properties, UUID, variant, target FPS, elapsed scene time, Runtime Inputs, and audio state. Most deterministic scenes need nothing more.

If a visible simulation reset would still occur, override:

```cpp
nlohmann::json snapshot_runtime_state() const override;
void restore_runtime_state(const nlohmann::json& state) override;
```

Keep the state compact and version-tolerant.

## Adaptive rendering and performance

The Pi records per-scene render p50/p95/p99 and the target frame budget. A scene may reduce optional work by reading:

```cpp
const float quality = render_quality_scale();
```

A known-heavy scene may start with a conservative hint:

```cpp
MyScene::MyScene()
{
    set_render_quality_hint(0.8f);
}
```

Good render-loop practices:

- avoid allocations every frame;
- cache invariant calculations;
- avoid unnecessary `sqrt`, `pow`, and trigonometry in per-pixel inner loops;
- use squared distance when possible;
- make optional detail respond to `render_quality_scale()`;
- use `FixedStepAccumulator` for physics that should not depend on render FPS;
- do not sleep inside the main render path. Central rendering owns pacing.

Diagnostics on the actual Pi are the authoritative performance signal. Emulator/x86 timings are useful for regressions, not for estimating absolute Pi speed.

## Previews

Portable scenes are previewable by default. A scene needing fixture data declares it:

```cpp
Previews::SceneSpec MyScene::get_preview_spec() const override
{
    return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
}
```

The plugin can register `Previews::DataProvider` instances via `create_preview_data_providers()`. Providers should publish/update the same state the real producer uses, so the render implementation remains identical.

Prefer `frame_context()` timing if you want deterministic `--virtual-time-only` previews.

Useful commands:

```bash
cmake --preset emulator -DSKIP_WEB_BUILD=ON
cmake --build --preset emulator -j4

./emulator_build/preview_gen \
  --scene my_scene \
  --frames 60 \
  --fps 30 \
  --virtual-time-only \
  --strict
```

## Desktop plugins

Desktop plugins derive from `Plugins::DesktopPlugin`. They are for real desktop-only functionality, such as audio capture or a configuration UI.

Common hooks include:

```cpp
void render() override;             // ImGui content
void post_init() override;
void pre_new_frame() override;
void before_exit() override;
void on_websocket_message(std::string message) override;
```

For UDP output, existing plugins may implement `compute_next_packet()`. Multi-datagram producers can implement `compute_next_packets()`.

The generic RenderOffload desktop plugin is different: it supervises the scene worker; it does not contain individual scene renderers.

## Matrix plugin lifecycle and communication

Available matrix-side hooks include:

```cpp
std::optional<std::string> before_server_init() override;
std::optional<std::string> after_server_init() override;
std::optional<std::string> pre_exit() override;
std::optional<std::vector<std::string>> on_websocket_open() override;
void on_websocket_message(const std::string& message) override;
bool on_udp_packet(uint8_t plugin_id, const uint8_t* data, size_t size) override;
std::unique_ptr<router_t> register_routes(std::unique_ptr<router_t> router) override;
```

Use `send_msg_to_desktop()` for plugin-scoped WebSocket messages.

Worker execution is marked by `SceneExecution::Mode::RemoteWorker`; preview execution uses `SceneExecution::Mode::Preview`. Plugins that normally start network/background services can use `SceneExecution::is_headless_fixture_host()` to suppress those services while still initializing scene-required state/resources.

## Image/shader providers, effects, and transitions

A matrix plugin can additionally override:

```cpp
create_image_providers();
create_shader_providers();
create_effects();
create_transitions();
```

Keep providers and integration code under `matrix/` unless they are part of the portable scene renderer itself.

Post-processing remains on the Pi even when a scene is remotely rendered. The worker renders the scene frame; the Pi remains responsible for transitions/post-processing/final matrix presentation.

## Building and testing

### Matrix/emulator

```bash
cmake --preset emulator -DSKIP_WEB_BUILD=ON
cmake --build --preset emulator -j4
ctest --test-dir emulator_build --output-on-failure
```

### Desktop + worker

```bash
cmake --preset desktop-linux -DSKIP_WEB_BUILD=ON
cmake --build --preset desktop-linux -j4

./desktop_build/led-matrix-scene-worker \
  --list-scenes \
  --plugin-dir ./desktop_build/scene_plugins
```

`--list-scenes` outputs one JSON array. Confirm your scene ID is present unless it intentionally sets `supports_remote_rendering = false`.

### Web

```bash
pnpm --dir react-web build
```

### Before committing a scene change

At minimum:

1. build the matrix/emulator target;
2. generate a strict preview for the scene;
3. run relevant CTest regressions;
4. build the desktop worker copy;
5. check `--list-scenes` contains the scene;
6. for performance-sensitive changes, inspect Pi Diagnostics rather than relying only on desktop timings.

## Current examples worth studying

- `plugins/ExampleScenes/` — minimal scene/plugin patterns.
- `plugins/AmbientScenes/` — procedural scenes and adaptive MetaBlob rendering.
- `plugins/GenerativeScenes/` — stateful/heavier simulations such as Boids and Reaction Diffusion.
- `plugins/AudioVisualizer/` — rich audio Runtime Input/state consumption.
- `plugins/SpotifyScenes/` — preview fixtures and mirrored network-backed runtime state.
- `plugins/Countdown/` and `plugins/WeatherOverview/` — plugin-local resources used by both matrix and worker builds.
- `plugins/RenderOffload/` — generic worker supervision and matrix-side transport.

## Common mistakes

- Putting new scene code in `matrix/scenes/` instead of `scenes/`.
- Writing a second desktop renderer for a portable scene.
- Making offload depend on a manually assigned `heavy=true` flag instead of measured Pi cost.
- Reading external data only through a process-local global when a Runtime Input would work.
- Using wall-clock timing for ordinary animations, causing preview/worker divergence.
- Adding a second frame sleep when the central renderer already owns pacing.
- Forgetting to package plugin-local fonts/images/assets for the worker plugin path.
- Opting out of remote rendering just because a scene is CPU-heavy.
