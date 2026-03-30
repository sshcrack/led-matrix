// WASM stubs: definitions of symbols that are needed for compilation but
// are not available in the browser environment (timing, image processing,
// canvas globals, provider base classes).

#include "led-matrix.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/config/image_providers/general.h"
#include "shared/matrix/config/shader_providers/general.h"
#include "shared/common/utils/utils.h"

#include <chrono>
#include <random>
#include <filesystem>

// ---------------------------------------------------------------------------
// Canvas constants (defined in shared/matrix/src/.../canvas_consts.cpp for
// native builds; we provide WASM-specific definitions here).
// ---------------------------------------------------------------------------

namespace Constants {
    std::atomic<int> width{128};
    std::atomic<int> height{128};
    PostProcessor    *global_post_processor   = nullptr;
    TransitionManager *global_transition_manager = nullptr;
}

// ---------------------------------------------------------------------------
// Common utility stubs
// ---------------------------------------------------------------------------

tmillis_t GetTimeInMillis() {
    return static_cast<tmillis_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

/// No-op in WASM: frame pacing is driven by the JavaScript render loop.
void SleepMillis(tmillis_t) {}

int get_random_number_inclusive(int start, int end) {
    static std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<> dist(start, end);
    return dist(engine);
}

bool try_remove(const std::filesystem::path &) { return false; }
bool is_valid_filename(const std::string &) { return true; }
bool replace(std::string &str, const std::string &from, const std::string &to) {
    size_t pos = str.find(from);
    if (pos == std::string::npos) return false;
    str.replace(pos, from.length(), to);
    return true;
}
std::string stringify_url(const std::string &url) {
    return std::to_string(std::hash<std::string>{}(url));
}
std::filesystem::path get_exec_file() { return {}; }
std::filesystem::path get_exec_dir()  { return {}; }

// ---------------------------------------------------------------------------
// Canvas pixel helpers (shared/matrix/utils/utils.h functions)
// ---------------------------------------------------------------------------

void floatPixelSet(rgb_matrix::FrameCanvas *canvas, int x, int y,
                   float r, float g, float b) {
    canvas->SetPixel(x, y,
                     static_cast<uint8_t>(r * 255.f),
                     static_cast<uint8_t>(g * 255.f),
                     static_cast<uint8_t>(b * 255.f));
}

void SetPixelAlpha(rgb_matrix::FrameCanvas *canvas, int x, int y,
                   uint8_t r, uint8_t g, uint8_t b, float alpha) {
    canvas->SetPixel(x, y,
                     static_cast<uint8_t>(r * alpha),
                     static_cast<uint8_t>(g * alpha),
                     static_cast<uint8_t>(b * alpha));
}

// ---------------------------------------------------------------------------
// ImageProviders::General base class stubs
// (general.cpp uses PluginManager which is not available in WASM)
// ---------------------------------------------------------------------------

ImageProviders::General::General()  = default;
ImageProviders::General::~General() = default;

void ImageProviders::General::load_properties(const nlohmann::json &j) {
    for (const auto &p : get_properties()) {
        p->load_from_json(j);
    }
}

nlohmann::json ImageProviders::General::to_json() const {
    nlohmann::json j;
    for (const auto &p : get_properties()) {
        p->dump_to_json(j);
    }
    return j;
}

std::unique_ptr<ImageProviders::General, void (*)(ImageProviders::General *)>
ImageProviders::General::from_json(const nlohmann::json &) {
    return {nullptr, [](ImageProviders::General *) {}};
}

// ---------------------------------------------------------------------------
// ShaderProviders::General base class stubs
// ---------------------------------------------------------------------------

ShaderProviders::General::General()  = default;
ShaderProviders::General::~General() = default;

void ShaderProviders::General::load_properties(const nlohmann::json &j) {
    for (const auto &p : get_properties()) {
        p->load_from_json(j);
    }
}

nlohmann::json ShaderProviders::General::to_json() const {
    nlohmann::json j;
    for (const auto &p : get_properties()) {
        p->dump_to_json(j);
    }
    return j;
}

std::unique_ptr<ShaderProviders::General, void (*)(ShaderProviders::General *)>
ShaderProviders::General::from_json(const nlohmann::json &) {
    return {nullptr, [](ShaderProviders::General *) {}};
}
