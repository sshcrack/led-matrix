#include "shared/matrix/Scene.h"

#include <shared/common/utils/utils.h>
#include <shared/matrix/utils/utils.h>
#include <shared/matrix/utils/uuid.h>
#include "spdlog/spdlog.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/plugin/property.h"
#include "shared/matrix/FallbackScene.h"
#include <shared/matrix/audio_state.h>
#include <shared/common/audio_protocol.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <unordered_set>
#include <magic_enum/magic_enum.hpp>

using namespace spdlog;

namespace {
std::string normalized_token(std::string value)
{
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value)
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

std::optional<AudioProtocol::Feature> parse_audio_feature(const std::string &name)
{
    const std::string wanted = normalized_token(name);
    for (const auto feature : magic_enum::enum_values<AudioProtocol::Feature>()) {
        if (feature == AudioProtocol::Feature::Count) continue;
        if (normalized_token(std::string(magic_enum::enum_name(feature))) == wanted)
            return feature;
    }
    return std::nullopt;
}

float normalized_audio_feature(const AudioState::Snapshot &audio, AudioProtocol::Feature feature)
{
    const float raw = audio.feature(feature);
    switch (feature) {
        case AudioProtocol::Feature::Bpm:
            return std::clamp((raw - 60.0f) / 120.0f, 0.0f, 1.0f);
        case AudioProtocol::Feature::StereoBalance:
        case AudioProtocol::Feature::StereoCorrelation:
        case AudioProtocol::Feature::EnergyTrend:
            return std::clamp(raw * 0.5f + 0.5f, 0.0f, 1.0f);
        default:
            return std::clamp(raw, 0.0f, 1.0f);
    }
}
}

std::unique_ptr<Scenes::Scene> Scenes::Scene::from_json(const nlohmann::json &j)
{
    if (!j.contains("type"))
        throw std::runtime_error(fmt::format("No scene type given for '{}'", j.dump()));

    const string t = j["type"].get<string>();
    const nlohmann::json &arguments = j.value("arguments", nlohmann::json::object());

    const auto pl = Plugins::PluginManager::instance();
    for (const auto &item : pl->get_scenes())
    {
        if (item->get_name() != t)
            continue;

        auto scene = item->create();

        spdlog::debug("Creating scene '{}'", t);
        scene->update_default_properties();
        scene->register_properties();

        if (const auto variant = j.value("variant", std::string{}); !variant.empty()) {
            const auto descriptor = scene->get_descriptor();
            if (find_variant(descriptor, variant) != nullptr)
                scene->apply_variant(variant);
            else if (arguments.is_object())
                scene->variant_id_ = variant; // saved/user variant; complete arguments remain authoritative
            else
                throw std::runtime_error(fmt::format("Unknown variant '{}' for scene '{}'", variant, t));
        }

        spdlog::debug("Loading properties for scene '{}'", t);
        scene->load_properties(arguments);
        if (j.contains("uuid"))
            scene->uuid = j["uuid"].get<string>();

        if (scene->uuid.empty())
            scene->uuid = uuid::generate_uuid_v4();
        return scene;
    }

    spdlog::warn("Unknown scene type '{}', returning FallbackScene", t);
    auto unknown_scene = std::make_unique<Scenes::FallbackScene>(t);

    unknown_scene->arguments = arguments;
    if (j.contains("uuid"))
        unknown_scene->uuid = j["uuid"].get<string>();

    if (unknown_scene->uuid.empty())
        unknown_scene->uuid = uuid::generate_uuid_v4();

    return unknown_scene;
}

void Scenes::Scene::initialize(int width, int height)
{
    if (initialized)
        return;

    matrix_width = width;
    matrix_height = height;
    initialized = true;
    reset_frame_clock();
}

bool Scenes::Scene::is_initialized() const
{
    return initialized;
}

nlohmann::json Scenes::Scene::to_json() const
{
    nlohmann::json j;
    for (const auto &item : properties)
    {
        item->dump_to_json(j);
    }

    return j;
}

Scenes::SceneInputSpec Scenes::Scene::get_effective_runtime_inputs() const
{
    auto spec = get_runtime_input_spec();
    const auto caps = get_capabilities();
    if (caps.requires_desktop)
        spec.require(RuntimeInputIds::Desktop);
    if (caps.requires_audio)
        spec.require(RuntimeInputIds::Audio);
    else if (caps.supports_audio)
        spec.accept(RuntimeInputIds::Audio);
    return spec;
}

Scenes::SceneDescriptor Scenes::Scene::get_descriptor() const
{
    SceneDescriptor descriptor;
    descriptor.family = get_category();
    const auto caps = get_capabilities();
    descriptor.music_affinity = caps.requires_audio ? 1.0f : (caps.supports_audio ? 0.65f : 0.1f);
    descriptor.automatic_eligible = false;
    if (caps.supports_audio)
        descriptor.tags.emplace_back("audio-reactive");
    if (caps.requires_network)
        descriptor.tags.emplace_back("network");
    return descriptor;
}

void Scenes::Scene::apply_variant(std::string_view id)
{
    if (id.empty()) {
        variant_id_.clear();
        return;
    }
    const auto descriptor = get_descriptor();
    const auto *variant = find_variant(descriptor, id);
    if (variant == nullptr)
        throw std::runtime_error(fmt::format("Unknown variant '{}' for scene '{}'", id, get_name()));
    load_properties(variant->properties);
    variant_id_ = variant->id;
}

tmillis_t Scenes::Scene::get_duration() const
{
    return duration->get();
}

tmillis_t Scenes::Scene::get_transition_duration() const
{
    return transition_duration->get();
}

std::string Scenes::Scene::get_transition_name() const
{
    return transition_name->get();
}

int Scenes::Scene::get_weight() const
{
    return weight->get();
}

void Scenes::Scene::wait_until_next_frame()
{
    if (suppress_internal_wait_)
        return;

    const tmillis_t step = std::max<tmillis_t>(1, 1000 / std::max(1, target_fps));
    const tmillis_t current_time = GetTimeInMillis();
    if (last_render_time == 0 || last_render_time + step <= current_time)
    {
        last_render_time = current_time;
        return;
    }

    const tmillis_t deadline = last_render_time + step;
    const auto wait_start = std::chrono::steady_clock::now();
    SleepMillis(deadline - current_time);
    frame_wait_ms_ += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - wait_start).count();
    last_render_time = deadline;
}


void Scenes::Scene::reset_frame_clock()
{
    frame_context_ = {};
    frame_clock_started_ = false;
    last_render_time = 0;
    frame_wait_ms_ = 0.0;
    render_quality_scale_ = 1.0f;
    render_over_budget_streak_ = 0;
    render_under_budget_streak_ = 0;
}

void Scenes::Scene::report_render_cost(double active_render_ms)
{
    const double budget_ms = 1000.0 / static_cast<double>(std::max(1, target_fps));
    if (active_render_ms > budget_ms * 0.92) {
        ++render_over_budget_streak_;
        render_under_budget_streak_ = 0;
        if (render_over_budget_streak_ >= 4) {
            render_quality_scale_ = std::max(0.50f, render_quality_scale_ - 0.06f);
            render_over_budget_streak_ = 0;
        }
    } else if (active_render_ms < budget_ms * 0.58) {
        ++render_under_budget_streak_;
        render_over_budget_streak_ = 0;
        if (render_under_budget_streak_ >= 75) {
            render_quality_scale_ = std::min(1.0f, render_quality_scale_ + 0.03f);
            render_under_budget_streak_ = 0;
        }
    } else {
        render_over_budget_streak_ = 0;
        render_under_budget_streak_ = 0;
    }
}

void Scenes::Scene::restore_audio_modulations()
{
    if (audio_modulation_state_.empty()) return;
    for (const auto &[name, state] : audio_modulation_state_) {
        const auto it = std::find_if(properties.begin(), properties.end(), [&](const auto &property) {
            return property && property->getName() == name;
        });
        if (it != properties.end() && (*it)->supports_runtime_numeric())
            (*it)->set_runtime_numeric_value(state.base_value);
    }
    audio_modulation_state_.clear();
}

void Scenes::Scene::apply_audio_modulations(double dt)
{
    const auto &bindings = audio_modulations_->get();
    if (!bindings.is_array() || bindings.empty()) {
        restore_audio_modulations();
        return;
    }

    const auto audio = AudioState::snapshot();
    if (!audio.fresh()) {
        restore_audio_modulations();
        return;
    }

    std::unordered_set<std::string> active_properties;
    for (const auto &binding : bindings) {
        if (!binding.is_object() || !binding.contains("property") || !binding.at("property").is_string() ||
            !binding.contains("feature") || !binding.at("feature").is_string())
            continue;
        const std::string property_name = binding.at("property").get<std::string>();
        const std::string feature_name = binding.at("feature").get<std::string>();
        if (property_name.empty() || feature_name.empty()) continue;
        if (property_name == "weight" || property_name == "duration" ||
            property_name == "transition_duration" || property_name == "audio_modulations") continue;

        const auto feature = parse_audio_feature(feature_name);
        if (!feature.has_value()) continue;
        const auto property_it = std::find_if(properties.begin(), properties.end(), [&](const auto &property) {
            return property && property->getName() == property_name;
        });
        if (property_it == properties.end() || !(*property_it)->supports_runtime_numeric()) continue;

        auto value = (*property_it)->runtime_numeric_value();
        if (!value.has_value()) continue;
        if (!active_properties.insert(property_name).second) continue;
        auto [state_it, inserted] = audio_modulation_state_.try_emplace(
            property_name, AudioModulationState{*value, *value});
        auto &state = state_it->second;

        const auto number = [&](const char *key, double fallback) {
            return binding.contains(key) && binding.at(key).is_number()
                ? binding.at(key).get<double>()
                : fallback;
        };
        const bool invert = binding.contains("invert") && binding.at("invert").is_boolean()
            ? binding.at("invert").get<bool>()
            : false;

        float signal = normalized_audio_feature(audio, *feature);
        if (invert) signal = 1.0f - signal;
        const double curve = std::clamp(number("curve", 1.0), 0.15, 4.0);
        signal = static_cast<float>(std::pow(signal, curve));

        const double low = number("min", state.base_value);
        const double high = number("max", state.base_value);
        const double target = low + (high - low) * static_cast<double>(signal);
        const double smoothing = std::clamp(number("smoothing", 0.12), 0.0, 3.0);
        const double alpha = smoothing <= 0.0001
            ? 1.0
            : 1.0 - std::exp(-std::clamp(dt, 0.0, 0.25) / smoothing);
        state.smoothed_value += (target - state.smoothed_value) * alpha;
        (*property_it)->set_runtime_numeric_value(state.smoothed_value);
    }

    for (auto it = audio_modulation_state_.begin(); it != audio_modulation_state_.end();) {
        if (active_properties.contains(it->first)) { ++it; continue; }
        const auto property_it = std::find_if(properties.begin(), properties.end(), [&](const auto &property) {
            return property && property->getName() == it->first;
        });
        if (property_it != properties.end() && (*property_it)->supports_runtime_numeric())
            (*property_it)->set_runtime_numeric_value(it->second.base_value);
        it = audio_modulation_state_.erase(it);
    }
}

bool Scenes::Scene::render_frame(FrameCanvas *canvas,
                                 std::optional<double> forced_delta_seconds,
                                 bool suppress_internal_wait)
{
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();

    double delta = 0.0;
    bool deterministic = forced_delta_seconds.has_value();
    if (forced_delta_seconds.has_value()) {
        delta = std::clamp(*forced_delta_seconds, 0.0, 0.25);
        if (!frame_clock_started_) {
            frame_clock_start_ = now;
            frame_clock_last_ = now;
            frame_clock_started_ = true;
        }
    } else if (!frame_clock_started_) {
        frame_clock_start_ = now;
        frame_clock_last_ = now;
        frame_clock_started_ = true;
        delta = 1.0 / static_cast<double>(std::max(1, target_fps));
    } else {
        delta = std::clamp(std::chrono::duration<double>(now - frame_clock_last_).count(), 0.0, 0.25);
        frame_clock_last_ = now;
    }

    frame_context_.delta_seconds = delta;
    frame_context_.elapsed_seconds += delta;
    frame_context_.frame_index += 1;
    frame_context_.now_ms = static_cast<std::uint64_t>(frame_context_.elapsed_seconds * 1000.0);
    frame_context_.deterministic = deterministic;

    frame_wait_ms_ = 0.0;
    frame_updated_ = true;
    const bool previous_suppress = suppress_internal_wait_;
    suppress_internal_wait_ = suppress_internal_wait || deterministic;
    try {
        apply_audio_modulations(delta);
        const bool result = render(canvas);
        suppress_internal_wait_ = previous_suppress;
        return result;
    } catch (...) {
        suppress_internal_wait_ = previous_suppress;
        throw;
    }
}

Scenes::Scene::Scene()
{
    weight->label("Playlist weight").description("Relative probability of this scene being selected from a preset.")
        .group("Playback").step(1).control("number").advanced();
    duration->label("Scene duration").description("How long this scene remains active before the playlist advances.")
        .group("Playback").unit("duration").control("duration")
        .presets(nlohmann::json::array({5000, 15000, 30000, 60000, 120000}));
    transition_duration->label("Transition duration").description("Blend time into the next scene. Use 0 for an immediate switch.")
        .group("Transition").unit("duration").control("duration")
        .presets(nlohmann::json::array({0, 150, 250, 500, 750, 1000, 2000}));
    transition_name->label("Transition style").description("Transition effect used when this scene ends.")
        .group("Transition").control("select");
    audio_modulations_->label("Audio modulation").description("Bind numeric scene settings to live music-analysis features. Audio loss automatically restores configured values.")
        .group("Audio modulation").control("audio_modulations").advanced();

    add_property(weight);
    add_property(duration);
    add_property(transition_duration);
    add_property(transition_name);
    add_property(audio_modulations_);
}

void Scenes::Scene::after_render_stop()
{
}

void Scenes::Scene::before_transition_stop()
{
}

void Scenes::Scene::load_properties(const json &j)
{
    for (const auto &item : properties)
    {
        item->load_from_json(j);
    }
}
