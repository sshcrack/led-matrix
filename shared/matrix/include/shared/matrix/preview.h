#pragma once

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include <nlohmann/json.hpp>

#include "shared/matrix/scene_runtime.h"

namespace Previews {

namespace Inputs {
inline constexpr std::string_view Audio = "audio";
inline constexpr std::string_view SpotifyPlayback = "spotify.playback";
} // namespace Inputs

/// Declarative preview contract for a scene. A scene only names the fixture
/// inputs it needs and optional property overrides; preview_gen resolves those
/// names to providers registered by plugins.
struct SceneSpec {
    bool enabled = true;
    std::vector<std::string> inputs;
    nlohmann::json property_overrides = nlohmann::json::object();

    static SceneSpec disabled() {
        SceneSpec spec;
        spec.enabled = false;
        return spec;
    }

    static SceneSpec with_inputs(std::initializer_list<std::string_view> requested) {
        SceneSpec spec;
        spec.inputs.reserve(requested.size());
        for (const auto input : requested)
            spec.inputs.emplace_back(input);
        return spec;
    }

    SceneSpec &set_property(std::string name, nlohmann::json value) {
        property_overrides[std::move(name)] = std::move(value);
        return *this;
    }
};

/// Common information made available to preview fixture providers. Arbitrary
/// provider-specific options are carried in `options`; preview_gen therefore
/// does not need to understand a plugin's data format.
struct RunContext {
    int width = 128;
    int height = 128;
    int fps = 15;
    int total_frames = 90;
    nlohmann::json options = nlohmann::json::object();

    [[nodiscard]] nlohmann::json options_for(std::string_view provider_id) const {
        if (!options.is_object()) return nlohmann::json::object();
        const auto it = options.find(std::string(provider_id));
        return it != options.end() && it->is_object() ? *it : nlohmann::json::object();
    }
};

/// Adapter supplied by a plugin for one named preview input. Providers update
/// the same state that production data sources update, so scene render code
/// remains identical between real playback and preview generation.
class DataProvider {
public:
    virtual ~DataProvider() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;
    virtual void begin(const RunContext &context) = 0;
    virtual void update(const Scenes::SceneFrameContext &frame) = 0;
    virtual void end() noexcept = 0;
};

/// Process-wide marker used only while preview_gen is loading/running plugins.
/// Plugins may use it to avoid credentials, network threads, or other runtime
/// setup that their preview provider replaces with deterministic fixture data.
class Runtime {
public:
    [[nodiscard]] static bool active();
    static void set_active(bool active);
};

class RuntimeScope {
public:
    RuntimeScope() { Runtime::set_active(true); }
    ~RuntimeScope() { Runtime::set_active(false); }

    RuntimeScope(const RuntimeScope &) = delete;
    RuntimeScope &operator=(const RuntimeScope &) = delete;
};

} // namespace Previews
