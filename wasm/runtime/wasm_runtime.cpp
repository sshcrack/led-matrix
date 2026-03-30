// WASM runtime entry point.
// Exposes the scene preview ABI to JavaScript via Emscripten's exported C API.
//
// JavaScript API:
//   const char* wasm_list_scenes()          – JSON array of scene metadata
//   int  wasm_scene_create(name, w, h)      – 0 = ok, non-zero = not found
//   void wasm_scene_set_properties(json)    – update scene properties at runtime
//   int  wasm_scene_render_frame()          – render one frame; 1=continue, 0=stop
//   void wasm_scene_destroy()               – release current scene
//   uint8_t* wasm_get_frame_buffer()        – pointer into WASM heap (RGBA data)
//   int      wasm_get_buffer_size()         – byte length of the frame buffer

#include <memory>
#include <vector>
#include <string>

// WASM canvas / scene headers
#include "led-matrix.h"
#include "shared/matrix/wrappers.h"

// ExampleScenes (WASM v1 target scenes)
#include "scenes/ColorPulseScene.h"
#include "scenes/PropertyDemoScene.h"
#include "scenes/RenderingDemoScene.h"

#include "nlohmann/json.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------

static std::vector<std::shared_ptr<Plugins::SceneWrapper>> g_registry;

static std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)>
    g_scene{nullptr, [](Scenes::Scene *) {}};

static std::unique_ptr<rgb_matrix::FrameCanvas> g_canvas;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void ensure_registry() {
    if (!g_registry.empty()) return;
    g_registry.push_back(std::make_shared<Scenes::ColorPulseSceneWrapper>());
    g_registry.push_back(std::make_shared<Scenes::PropertyDemoSceneWrapper>());
    g_registry.push_back(std::make_shared<Scenes::RenderingDemoSceneWrapper>());
}

/// Build a JSON object containing all default property values for a scene.
/// This is used to initialise registered=true on every property before
/// the first call to wasm_scene_set_properties().
static nlohmann::json default_props_json(Scenes::Scene &scene) {
    nlohmann::json j;
    for (const auto &p : scene.get_properties()) {
        p->dump_to_json(j);
    }
    return j;
}

// ---------------------------------------------------------------------------
// Exported C API
// ---------------------------------------------------------------------------

extern "C" {

/// Returns a JSON array of scene descriptors:
/// [{"name":"...", "needs_desktop":false, "properties":[{...}]}, ...]
WASM_EXPORT const char *wasm_list_scenes() {
    ensure_registry();
    static std::string result_buf;

    nlohmann::json arr = nlohmann::json::array();
    for (auto &wrapper : g_registry) {
        // get_default() caches a single default instance per wrapper.
        auto def = wrapper->get_default();

        nlohmann::json entry;
        entry["name"]         = def->get_name();
        entry["needs_desktop"] = def->needs_desktop_app();

        nlohmann::json props = nlohmann::json::array();
        for (const auto &p : def->get_properties()) {
            nlohmann::json prop;
            prop["name"]    = p->getName();
            prop["type_id"] = p->get_type_id();

            // Serialise the current (default) value
            nlohmann::json val_obj;
            p->dump_to_json(val_obj);
            prop["default_value"] = val_obj.value(p->getName(), nlohmann::json{});

            nlohmann::json additional;
            p->add_additional_data(additional);
            prop["additional"] = additional;

            props.push_back(prop);
        }
        entry["properties"] = props;
        arr.push_back(entry);
    }

    result_buf = arr.dump();
    return result_buf.c_str();
}

/// Create and initialise a scene by name.
/// Returns 0 on success, 1 if the scene name is not found in the registry.
WASM_EXPORT int wasm_scene_create(const char *scene_name, int width, int height) {
    ensure_registry();
    const std::string name(scene_name);

    for (auto &wrapper : g_registry) {
        if (wrapper->get_name() != name) continue;

        auto scene = wrapper->create();
        scene->update_default_properties();
        scene->register_properties();

        // Load defaults so all properties are in the "registered" state
        // and get() does not throw when render() accesses them.
        scene->load_properties(default_props_json(*scene));

        scene->initialize(width, height);

        g_canvas = std::make_unique<rgb_matrix::FrameCanvas>(width, height);
        g_scene  = std::move(scene);
        return 0;
    }
    return 1; // scene not found
}

/// Apply property values from a JSON string.
/// Only the keys present in the JSON are updated; others keep their current values.
WASM_EXPORT void wasm_scene_set_properties(const char *json_str) {
    if (!g_scene) return;
    try {
        const auto j = nlohmann::json::parse(json_str);
        g_scene->load_properties(j);
    } catch (...) {
        // Silently ignore malformed JSON or unknown property names.
    }
}

/// Render one frame into the internal RGBA buffer.
/// Returns 1 if the scene wants to continue, 0 if it has finished.
WASM_EXPORT int wasm_scene_render_frame() {
    if (!g_scene || !g_canvas) return 0;
    return g_scene->render(g_canvas.get()) ? 1 : 0;
}

/// Destroy the current scene and release its frame buffer.
WASM_EXPORT void wasm_scene_destroy() {
    g_scene  = {nullptr, [](Scenes::Scene *) {}};
    g_canvas.reset();
}

/// Returns a pointer into WASM heap memory pointing to the RGBA frame buffer.
/// The caller must NOT free this pointer; it is owned by the module.
WASM_EXPORT uint8_t *wasm_get_frame_buffer() {
    if (!g_canvas) return nullptr;
    return g_canvas->data();
}

/// Returns the byte length of the frame buffer (width × height × 4).
WASM_EXPORT int wasm_get_buffer_size() {
    if (!g_canvas) return 0;
    return static_cast<int>(g_canvas->size());
}

} // extern "C"
