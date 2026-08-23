#include "MusicDirectorScene.h"

#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/diagnostics.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/runtime_inputs.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <nlohmann/json.hpp>

namespace Scenes {
namespace {
float feature(const AudioState::Snapshot& audio, AudioProtocol::Feature id)
{
    return std::clamp(audio.feature(id), 0.0f, 1.0f);
}

float smoothResponse(float previous, float target, float attackSeconds, float releaseSeconds, float dt)
{
    const float time = target > previous ? attackSeconds : releaseSeconds;
    const float alpha = 1.0f - std::exp(-std::max(0.0f, dt) / std::max(0.001f, time));
    return previous + (target - previous) * alpha;
}

bool hasTag(const Scenes::EffectiveSceneProfile& profile, std::string_view tag)
{
    return std::find(profile.tags.begin(), profile.tags.end(), tag) != profile.tags.end();
}

float eased(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}
}  // namespace

void MusicDirectorScene::register_properties()
{
    scene_pool_->label("Scene pool")
        .description("Scenes Music Director may choose from. Unsupported or unavailable entries are ignored.")
        .group("Director");
    minimum_dwell_->label("Minimum scene time")
        .description("Do not switch again before this much time has passed.")
        .group("Timing")
        .control("duration");
    maximum_dwell_->label("Maximum scene time")
        .description("Force fresh visual variety after this time even without a section change.")
        .group("Timing")
        .control("duration");
    beat_sync_->label("Beat-synchronised changes")
        .description("When tempo tracking is confident, wait for a beat before changing scenes.")
        .group("Timing");
    beat_quantization_->label("Beat quantization")
        .description("When beat sync is reliable, align ordinary scene changes to a beat, 2 beats, a 4-beat bar, or two bars.")
        .group("Timing")
        .visible_if("beat_sync", true);
    react_on_sections_->label("React to section changes")
        .description("Use detected structural changes as scene-change opportunities.")
        .group("Musical intelligence");
    react_on_drops_->label("React to drops")
        .description("Move into a high-energy visual immediately around detected drops.")
        .group("Musical intelligence");
    configure_child_audio_->label("Configure child audio reactivity")
        .description("Automatically enable audio_reactive/audio_strength properties on compatible scenes.")
        .group("Child scenes");
    child_audio_strength_->label("Child audio strength")
        .description("Audio-reactive strength applied to compatible child scenes.")
        .group("Child scenes")
        .visible_if("configure_child_audio", true)
        .step(0.05);
    switch_effects_->label("Musical switch effects")
        .description("Use lightweight post-processing accents when Music Director changes visual energy state.")
        .group("Musical intelligence");
    spotify_artwork_colors_->label("Spotify artwork colors")
        .description(
            "When Spotify has published current cover colors, let compatible child scenes use them. Track changes cross-fade without "
            "restarting the scene.")
        .group("Child scenes");

    add_property(scene_pool_);
    add_property(minimum_dwell_);
    add_property(maximum_dwell_);
    add_property(beat_sync_);
    add_property(beat_quantization_);
    add_property(react_on_sections_);
    add_property(react_on_drops_);
    add_property(configure_child_audio_);
    add_property(child_audio_strength_);
    add_property(switch_effects_);
    add_property(spotify_artwork_colors_);
}

void MusicDirectorScene::initialize(int width, int height)
{
    Scene::initialize(width, height);
    switched_at_ = frame_context().elapsed_seconds;
    state_energy_ = 0.25f;
    events_primed_ = false;
    last_frame_.clear();
    transition_from_frame_.clear();
    child_transition_started_at_ = -1.0;
}

void MusicDirectorScene::capture_frame(rgb_matrix::FrameCanvas* canvas, std::vector<rgb_matrix::Color>& target) const
{
    if (!canvas)
        return;
    target.resize(static_cast<size_t>(matrix_width * matrix_height));
    for (int y = 0; y < matrix_height; ++y)
        for (int x = 0; x < matrix_width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            canvas->GetPixel(x, y, &r, &g, &b);
            target[static_cast<size_t>(y * matrix_width + x)] = rgb_matrix::Color(r, g, b);
        }
}

void MusicDirectorScene::restore_frame(rgb_matrix::FrameCanvas* canvas, const std::vector<rgb_matrix::Color>& source) const
{
    if (!canvas || source.size() != static_cast<size_t>(matrix_width * matrix_height))
        return;
    for (int y = 0; y < matrix_height; ++y)
        for (int x = 0; x < matrix_width; ++x) {
            const auto& c = source[static_cast<size_t>(y * matrix_width + x)];
            canvas->SetPixel(x, y, c.r, c.g, c.b);
        }
}

void MusicDirectorScene::blend_child_transition(rgb_matrix::FrameCanvas* canvas)
{
    if (!canvas || child_transition_started_at_ < 0.0 || child_transition_duration_ <= 0.0 ||
        transition_from_frame_.size() != static_cast<size_t>(matrix_width * matrix_height))
        return;
    const float progress =
        static_cast<float>((frame_context().elapsed_seconds - child_transition_started_at_) / child_transition_duration_);
    if (progress >= 1.0f) {
        transition_from_frame_.clear();
        child_transition_started_at_ = -1.0;
        return;
    }
    const float mix = eased(progress);
    for (int y = 0; y < matrix_height; ++y)
        for (int x = 0; x < matrix_width; ++x) {
            uint8_t nr = 0, ng = 0, nb = 0;
            canvas->GetPixel(x, y, &nr, &ng, &nb);
            const auto& old = transition_from_frame_[static_cast<size_t>(y * matrix_width + x)];
            auto channel = [mix](uint8_t a, uint8_t b) {
                return static_cast<uint8_t>(
                    std::clamp(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * mix, 0.0f, 255.0f));
            };
            canvas->SetPixel(x, y, channel(old.r, nr), channel(old.g, ng), channel(old.b, nb));
        }
}

MusicDirectorScene::MusicalState MusicDirectorScene::classify(const AudioState::Snapshot& audio)
{
    if (!audio.fresh() || feature(audio, AudioProtocol::Feature::Silence) > 0.72f) {
        state_energy_ = smoothResponse(state_energy_, 0.16f, 0.35f, 0.70f, static_cast<float>(frame_context().delta_seconds));
        return MusicalState::Calm;
    }

    const float loud = feature(audio, AudioProtocol::Feature::LoudnessFast);
    const float bass = 0.55f * feature(audio, AudioProtocol::Feature::Bass) + 0.45f * feature(audio, AudioProtocol::Feature::SubBass);
    const float onset = feature(audio, AudioProtocol::Feature::OnsetStrength);
    const float hihat = feature(audio, AudioProtocol::Feature::Hihat);
    const float trend = std::clamp(audio.feature(AudioProtocol::Feature::EnergyTrend), -1.0f, 1.0f);
    const float target =
        std::clamp(0.08f + loud * 0.52f + bass * 0.22f + onset * 0.10f + hihat * 0.04f + std::max(0.0f, trend) * 0.12f, 0.0f, 1.0f);
    state_energy_ = smoothResponse(state_energy_, target, 0.28f, 0.85f, static_cast<float>(frame_context().delta_seconds));

    // Verified drop events may react immediately; ordinary energy changes must
    // survive temporal smoothing before they can change the visual state.
    if (feature(audio, AudioProtocol::Feature::Drop) > 0.48f)
        return MusicalState::Peak;
    if (state_energy_ > 0.76f)
        return MusicalState::Peak;
    if (state_energy_ > 0.59f || (trend > 0.20f && state_energy_ > 0.46f))
        return MusicalState::Build;
    if (state_energy_ < 0.29f)
        return MusicalState::Calm;
    return MusicalState::Groove;
}

std::vector<std::string> MusicDirectorScene::preferred(MusicalState state) const
{
    switch (state) {
        case MusicalState::Calm:
            return {"audio_aurora", "audio_spectrum", "starfield", "metablob", "reaction_diffusion"};
        case MusicalState::Build:
            return {"wave_pattern", "boids", "audio_spectrum", "neontunnel", "audio_pulse_tunnel"};
        case MusicalState::Peak:
            return {"audio_pulse_tunnel", "audio_particles", "audio_kaleidoscope", "audio_spectrum", "neontunnel"};
        case MusicalState::Groove:
        default:
            return {"boids", "audio_spectrum", "wave_pattern", "metablob", "audio_kaleidoscope", "audio_particles"};
    }
}

bool MusicDirectorScene::child_allowed(const std::string& name) const
{
    if (name.empty() || name == get_name())
        return false;
    const auto& pool = scene_pool_->get();
    if (!pool.empty() && std::find(pool.begin(), pool.end(), name) == pool.end())
        return false;

    for (const auto& wrapper : Plugins::PluginManager::instance()->get_scenes()) {
        if (!wrapper || wrapper->get_name() != name)
            continue;
        const auto scene = wrapper->get_default();
        if (!scene)
            return false;
        const auto caps = scene->get_capabilities();
        // The outer active scene remains music_director, so desktop-dependent
        // scenes that require their own plugin packets cannot be nested safely.
        if (caps.requires_desktop && !caps.requires_audio)
            return false;
        if (!caps.music_director_eligible || caps.interactive)
            return false;
        return RuntimeInputs::satisfies(scene->get_effective_runtime_inputs(), RuntimeInputs::snapshot());
    }
    return false;
}

void MusicDirectorScene::stop_child() noexcept
{
    if (!child_)
        return;
    try {
        child_->after_render_stop();
    }
    catch (const std::exception& e) {
        spdlog::warn("Music Director child '{}' cleanup failed: {}", child_name_, e.what());
    }
    catch (...) {
        spdlog::warn("Music Director child '{}' cleanup failed with unknown exception", child_name_);
    }
    child_.reset();
    child_name_.clear();
    child_variant_.clear();
}

bool MusicDirectorScene::switch_child(MusicalState state, const AudioState::Snapshot& audio)
{
    auto candidates = preferred(state);
    const auto& pool = scene_pool_->get();
    for (const auto& name : pool)
        if (std::find(candidates.begin(), candidates.end(), name) == candidates.end())
            candidates.push_back(name);
    if (candidates.empty())
        return false;

    const float loud = feature(audio, AudioProtocol::Feature::LoudnessFast);
    const float bass = 0.55f * feature(audio, AudioProtocol::Feature::Bass) + 0.45f * feature(audio, AudioProtocol::Feature::SubBass);
    const float treble = 0.60f * feature(audio, AudioProtocol::Feature::Treble) + 0.40f * feature(audio, AudioProtocol::Feature::Hihat);
    const float onset = feature(audio, AudioProtocol::Feature::OnsetStrength);
    const float confidence = feature(audio, AudioProtocol::Feature::BeatConfidence);
    const float stability = feature(audio, AudioProtocol::Feature::TempoStability);
    const float tempoTrust = std::clamp((confidence - 0.30f) / 0.50f, 0.0f, 1.0f) * stability;
    const float trend = std::clamp(audio.feature(AudioProtocol::Feature::EnergyTrend), -1.0f, 1.0f);

    const float stateIntensity = state == MusicalState::Calm     ? 0.26f
                                 : state == MusicalState::Groove ? 0.52f
                                 : state == MusicalState::Build  ? 0.72f
                                                                 : 0.90f;
    float targetIntensity =
        std::clamp(stateIntensity * 0.68f + (0.16f + loud * 0.72f) * 0.32f + std::max(0.0f, trend) * 0.08f, 0.16f, 0.96f);
    if (state == MusicalState::Peak)
        targetIntensity = std::max(targetIntensity, 0.84f);
    if (state == MusicalState::Calm)
        targetIntensity = std::min(targetIntensity, 0.38f);
    float targetMotion = std::clamp(0.22f + loud * 0.26f + onset * 0.18f + treble * 0.12f + tempoTrust * 0.20f, 0.18f, 0.94f);
    if (state == MusicalState::Calm)
        targetMotion = std::min(targetMotion, 0.44f);
    if (state == MusicalState::Peak)
        targetMotion = std::max(targetMotion, 0.76f);

    struct Choice {
        std::string name;
        std::string variant;
        float score = -1000.0f;
        size_t candidate_index = 0;
    };
    std::vector<Choice> choices;
    auto wrappers = Plugins::PluginManager::instance()->get_scenes();
    const std::string currentFamily = child_ ? child_->get_descriptor().family : std::string{};

    for (size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex) {
        const auto& name = candidates[candidateIndex];
        if (name == child_name_ || !child_allowed(name))
            continue;
        const auto wrapper =
            std::find_if(wrappers.begin(), wrappers.end(), [&](const auto& item) { return item && item->get_name() == name; });
        if (wrapper == wrappers.end())
            continue;
        const auto descriptor = (*wrapper)->get_default()->get_descriptor();

        auto scoreProfile = [&](const Scenes::SceneVariant* variant) {
            const auto profile = Scenes::effective_profile(descriptor, variant);
            float score = 3.2f;
            score -= std::abs(profile.intensity - targetIntensity) * 2.25f;
            score -= std::abs(profile.motion - targetMotion) * 1.25f;
            score += profile.music_affinity * 0.92f;
            score -= profile.performance_cost * 0.42f;

            // Musical timbre steers *style*, while energy/motion select the
            // variant. This keeps automatic choices intentional without any
            // scene-specific property knowledge.
            if (hasTag(profile, "depth") || hasTag(profile, "tunnel"))
                score += bass * 0.30f;
            if (hasTag(profile, "particles"))
                score += (onset * 0.18f + treble * 0.20f);
            if (hasTag(profile, "ribbons") || hasTag(profile, "flow"))
                score += (1.0f - onset) * 0.14f + treble * 0.10f;
            if (hasTag(profile, "organic"))
                score += (1.0f - tempoTrust) * 0.10f + (1.0f - loud) * 0.08f;
            if (hasTag(profile, "geometric") || hasTag(profile, "symmetry"))
                score += tempoTrust * 0.16f;
            if (hasTag(profile, "waveform") || hasTag(profile, "minimal"))
                score += (1.0f - loud) * 0.12f;

            if (state == MusicalState::Calm && (hasTag(profile, "calm") || hasTag(profile, "soft") || hasTag(profile, "minimal")))
                score += 0.28f;
            if (state == MusicalState::Groove && (hasTag(profile, "flow") || hasTag(profile, "beat-driven") || hasTag(profile, "music")))
                score += 0.16f;
            if (state == MusicalState::Build && (hasTag(profile, "vivid") || hasTag(profile, "depth") || hasTag(profile, "dense")))
                score += 0.22f;
            if (state == MusicalState::Peak && (hasTag(profile, "energetic") || hasTag(profile, "vivid") || hasTag(profile, "dense")))
                score += 0.34f;

            if (!currentFamily.empty() && descriptor.family == currentFamily)
                score -= 0.34f;
            for (size_t i = 0; i < recent_children_.size(); ++i) {
                if (recent_children_[recent_children_.size() - 1 - i] != name)
                    continue;
                score -= i == 0 ? 0.95f : (i == 1 ? 0.55f : 0.28f);
                break;
            }
            // Preserve deterministic variety for nearly equivalent choices.
            const size_t rotationDistance = (candidateIndex + candidates.size() - selection_cursor_) % candidates.size();
            score -= static_cast<float>(rotationDistance) * 0.008f;
            return score;
        };

        if (descriptor.variants.empty()) {
            choices.push_back({name, {}, scoreProfile(nullptr), candidateIndex});
        }
        else {
            for (const auto& variant : descriptor.variants) choices.push_back({name, variant.id, scoreProfile(&variant), candidateIndex});
        }
    }

    std::stable_sort(choices.begin(), choices.end(), [](const Choice& a, const Choice& b) {
        if (std::abs(a.score - b.score) > 0.0001f)
            return a.score > b.score;
        if (a.name != b.name)
            return a.name < b.name;
        return a.variant < b.variant;
    });

    for (const auto& choice : choices) {
        const auto wrapper =
            std::find_if(wrappers.begin(), wrappers.end(), [&](const auto& item) { return item && item->get_name() == choice.name; });
        if (wrapper == wrappers.end())
            continue;
        try {
            auto next = (*wrapper)->create();
            if (!next)
                continue;
            next->update_default_properties();
            next->register_properties();
            if (!choice.variant.empty())
                next->apply_variant(choice.variant);

            nlohmann::json arguments = nlohmann::json::object();
            for (const auto& property : next->get_properties()) {
                if (!property)
                    continue;
                if (configure_child_audio_->get()) {
                    if (property->getName() == "audio_reactive")
                        arguments["audio_reactive"] = true;
                    if (property->getName() == "audio_strength")
                        arguments["audio_strength"] = child_audio_strength_->get();
                }
                if (property->getName() == "use_spotify_artwork" && spotify_artwork_colors_->get())
                    arguments["use_spotify_artwork"] = true;
            }
            next->load_properties(arguments);
            next->initialize(matrix_width, matrix_height);

            if (!last_frame_.empty()) {
                transition_from_frame_ = last_frame_;
                child_transition_started_at_ = frame_context().elapsed_seconds;
                child_transition_duration_ = state == MusicalState::Peak ? 0.34 : state == MusicalState::Build ? 0.44 : 0.58;
            }
            if (!child_name_.empty()) {
                recent_children_.push_back(child_name_);
                while (recent_children_.size() > 4) recent_children_.pop_front();
            }
            stop_child();
            child_variant_ = next->get_variant_id();
            child_ = std::move(next);
            child_name_ = choice.name;
            child_state_ = state;
            switched_at_ = frame_context().elapsed_seconds;
            pending_switch_ = false;
            selection_cursor_ = (choice.candidate_index + 1) % candidates.size();

            // Cross-fading owns the visual handoff. Optional accents are now
            // restrained, non-glitch effects and never whiten the whole frame.
            if (switch_effects_->get() && Constants::global_post_processor) {
                if (state == MusicalState::Groove)
                    Constants::global_post_processor->add_effect("glow", 0.44f, 0.08f);
                else if (state == MusicalState::Build)
                    Constants::global_post_processor->add_effect("shockwave", 0.48f, 0.16f);
                else if (state == MusicalState::Peak)
                    Constants::global_post_processor->add_effect("shockwave", 0.54f, 0.22f);
            }
            spdlog::info("Music Director selected '{}' variant '{}' score {:.2f}", child_name_, child_variant_, choice.score);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::warn("Music Director could not start '{}:{}': {}", choice.name, choice.variant, e.what());
        }
    }
    return false;
}

bool MusicDirectorScene::request_switch(const AudioState::Snapshot& audio, MusicalState state)
{
    const double dwell_ms = (frame_context().elapsed_seconds - switched_at_) * 1000.0;
    bool drop = false, section = false, newBeat = false;
    if (!events_primed_) {
        seen_drop_ = audio.drop_counter;
        seen_section_ = audio.section_counter;
        seen_beat_ = audio.beat_counter;
        events_primed_ = true;
    }
    else {
        if (audio.drop_counter < seen_drop_)
            seen_drop_ = audio.drop_counter;
        else
            drop = react_on_drops_->get() && audio.drop_counter > seen_drop_;
        if (audio.section_counter < seen_section_)
            seen_section_ = audio.section_counter;
        else
            section = react_on_sections_->get() && audio.section_counter > seen_section_;
        if (audio.beat_counter < seen_beat_)
            seen_beat_ = audio.beat_counter;
        else
            newBeat = audio.beat_counter > seen_beat_;
        seen_drop_ = audio.drop_counter;
        seen_section_ = audio.section_counter;
        seen_beat_ = audio.beat_counter;
    }

    const bool maxed = dwell_ms >= static_cast<double>(maximum_dwell_->get());
    const bool changed_energy = state != child_state_ && dwell_ms >= static_cast<double>(minimum_dwell_->get());

    if (!child_) {
        pending_switch_ = true;
        pending_state_ = state;
    }
    else if (drop && dwell_ms >= std::min<double>(minimum_dwell_->get(), 3500.0)) {
        pending_switch_ = true;
        pending_state_ = MusicalState::Peak;
    }
    else if ((section || maxed || changed_energy) && dwell_ms >= static_cast<double>(minimum_dwell_->get())) {
        pending_switch_ = true;
        pending_state_ = state;
    }

    if (!pending_switch_)
        return false;

    const bool confidentTempo =
        feature(audio, AudioProtocol::Feature::BeatConfidence) >= 0.55f && feature(audio, AudioProtocol::Feature::TempoStability) >= 0.48f;
    if (beat_sync_->get() && confidentTempo && child_ && !drop) {
        if (!newBeat)
            return false;
        const auto quantization = beat_quantization_->get().get();
        const uint64_t beats = std::max<uint64_t>(1, static_cast<uint64_t>(quantization));
        if ((audio.beat_counter % beats) != 0)
            return false;
    }
    return switch_child(pending_state_, audio);
}

bool MusicDirectorScene::render(rgb_matrix::FrameCanvas* canvas)
{
    const auto audio = AudioState::snapshot();
    if (!audio.fresh()) {
        // Hold the last valid frame across brief UDP jitter. Runtime Input TTL
        // will move Automatic Mode elsewhere if audio is actually gone.
        if (!last_frame_.empty())
            restore_frame(canvas, last_frame_);
        else
            canvas->Clear();
        return true;
    }

    const auto state = classify(audio);
    if (child_ && frame_context().elapsed_seconds >= next_runtime_input_check_) {
        next_runtime_input_check_ = frame_context().elapsed_seconds + 0.25;
        if (!RuntimeInputs::satisfies(child_->get_effective_runtime_inputs(), RuntimeInputs::snapshot())) {
            spdlog::debug("Music Director child '{}' lost a required Runtime Input; selecting a replacement", child_name_);
            stop_child();
            pending_switch_ = true;
            pending_state_ = state;
        }
    }
    request_switch(audio, state);
    if (!child_ && !switch_child(state, audio)) {
        if (!last_frame_.empty())
            restore_frame(canvas, last_frame_);
        else
            canvas->Clear();
        return true;
    }

    try {
        const bool keep = child_->render_frame(canvas, frame_context().delta_seconds, true);
        blend_child_transition(canvas);
        capture_frame(canvas, last_frame_);
        if (!keep) {
            pending_switch_ = true;
            pending_state_ = state;
        }
    }
    catch (const std::exception& e) {
        Diagnostics::RuntimeDiagnostics::instance().record_scene_error("music_director/" + child_name_, e.what());
        spdlog::error("Music Director child '{}' render failed: {}", child_name_, e.what());
        stop_child();
        pending_switch_ = true;
        pending_state_ = state;
        if (!last_frame_.empty())
            restore_frame(canvas, last_frame_);
        else
            canvas->Clear();
    }
    catch (...) {
        Diagnostics::RuntimeDiagnostics::instance().record_scene_error("music_director/" + child_name_, "unknown exception");
        spdlog::error("Music Director child '{}' render failed with unknown exception", child_name_);
        stop_child();
        pending_switch_ = true;
        pending_state_ = state;
        if (!last_frame_.empty())
            restore_frame(canvas, last_frame_);
        else
            canvas->Clear();
    }
    return true;
}

void MusicDirectorScene::after_render_stop()
{
    stop_child();
}
void MusicDirectorScene::before_transition_stop()
{
    if (!child_)
        return;
    try {
        child_->before_transition_stop();
    }
    catch (...) {
    }
}

}  // namespace Scenes

Scenes::SceneDescriptor Scenes::MusicDirectorScene::get_descriptor() const
{
    auto d = Scene::get_descriptor();
    d.automatic_eligible = true;
    d.family = "music-director";
    d.tags = {"music", "director", "adaptive", "audio-reactive"};
    d.intensity = 0.70f;
    d.motion = 0.72f;
    d.music_affinity = 1.0f;
    d.performance_cost = 0.62f;
    d.automatic_eligible = true;
    return d;
}
