// WASM-specific Scene.cpp replacement.
// Provides Scene lifecycle methods without depending on PluginManager,
// FallbackScene, or restinio (none of which are available in WASM builds).

#include "shared/matrix/Scene.h"
#include "shared/common/utils/utils.h"
#include "shared/matrix/utils/uuid.h"

using namespace spdlog;

// ---------------------------------------------------------------------------
// Scene constructor / destructor
// ---------------------------------------------------------------------------

Scenes::Scene::Scene() {
    add_property(weight);
    add_property(duration);
    add_property(transition_duration);
    add_property(transition_name);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Scenes::Scene::initialize(int width, int height) {
    if (initialized) return;
    matrix_width  = width;
    matrix_height = height;
    initialized   = true;
}

bool Scenes::Scene::is_initialized() const {
    return initialized;
}

void Scenes::Scene::after_render_stop()       {}
void Scenes::Scene::before_transition_stop()  {}

// ---------------------------------------------------------------------------
// Property access
// ---------------------------------------------------------------------------

nlohmann::json Scenes::Scene::to_json() const {
    nlohmann::json j;
    for (const auto &item : properties) {
        item->dump_to_json(j);
    }
    return j;
}

void Scenes::Scene::load_properties(const nlohmann::json &j) {
    for (const auto &item : properties) {
        item->load_from_json(j);
    }
}

void Scenes::Scene::update_default_properties() {
    weight->set_value(get_default_weight());
    duration->set_value(get_default_duration());
}

tmillis_t Scenes::Scene::get_duration() const           { return duration->get(); }
tmillis_t Scenes::Scene::get_transition_duration() const{ return transition_duration->get(); }
std::string Scenes::Scene::get_transition_name() const  { return transition_name->get(); }
int Scenes::Scene::get_weight() const                   { return weight->get(); }

// ---------------------------------------------------------------------------
// Frame timing (no-op: JavaScript drives the render loop in WASM builds)
// ---------------------------------------------------------------------------

void Scenes::Scene::wait_until_next_frame() {
    // Intentionally empty – pacing is handled by the browser's setInterval.
}

// ---------------------------------------------------------------------------
// from_json is not used in WASM builds (scenes are created directly by name).
// Providing a stub avoids linker errors if the symbol is referenced.
// ---------------------------------------------------------------------------

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)>
Scenes::Scene::from_json(const nlohmann::json &) {
    return {nullptr, [](Scenes::Scene *) {}};
}
