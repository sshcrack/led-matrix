#include "MusicDirectorScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/diagnostics.h>
#include <spdlog/spdlog.h>

namespace Scenes {
namespace {
float feature(const AudioState::Snapshot &audio, AudioProtocol::Feature id) {
    return std::clamp(audio.feature(id), 0.0f, 1.0f);
}
}

void MusicDirectorScene::register_properties() {
    scene_pool_->label("Scene pool").description("Scenes Music Director may choose from. Unsupported or unavailable entries are ignored.").group("Director");
    minimum_dwell_->label("Minimum scene time").description("Do not switch again before this much time has passed.").group("Timing").control("duration");
    maximum_dwell_->label("Maximum scene time").description("Force fresh visual variety after this time even without a section change.").group("Timing").control("duration");
    beat_sync_->label("Beat-synchronised changes").description("When tempo tracking is confident, wait for a beat before changing scenes.").group("Timing");
    react_on_sections_->label("React to section changes").description("Use detected structural changes as scene-change opportunities.").group("Musical intelligence");
    react_on_drops_->label("React to drops").description("Move into a high-energy visual immediately around detected drops.").group("Musical intelligence");
    configure_child_audio_->label("Configure child audio reactivity").description("Automatically enable audio_reactive/audio_strength properties on compatible scenes.").group("Child scenes");
    child_audio_strength_->label("Child audio strength").description("Audio-reactive strength applied to compatible child scenes.").group("Child scenes").visible_if("configure_child_audio", true).step(0.05);

    add_property(scene_pool_);
    add_property(minimum_dwell_);
    add_property(maximum_dwell_);
    add_property(beat_sync_);
    add_property(react_on_sections_);
    add_property(react_on_drops_);
    add_property(configure_child_audio_);
    add_property(child_audio_strength_);
}

void MusicDirectorScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    switched_at_ = frame_context().elapsed_seconds;
}

MusicDirectorScene::MusicalState MusicDirectorScene::classify(const AudioState::Snapshot &audio) const {
    if (!audio.fresh() || feature(audio, AudioProtocol::Feature::Silence) > 0.72f)
        return MusicalState::Calm;

    const float loud = feature(audio, AudioProtocol::Feature::LoudnessFast);
    const float bass = 0.55f * feature(audio, AudioProtocol::Feature::Bass) + 0.45f * feature(audio, AudioProtocol::Feature::SubBass);
    const float trend = audio.feature(AudioProtocol::Feature::EnergyTrend);
    const float onset = feature(audio, AudioProtocol::Feature::OnsetStrength);

    if (feature(audio, AudioProtocol::Feature::Drop) > 0.45f || (loud > 0.72f && bass > 0.62f))
        return MusicalState::Peak;
    if (trend > 0.16f && (loud > 0.35f || onset > 0.35f))
        return MusicalState::Build;
    if (loud < 0.30f && bass < 0.35f)
        return MusicalState::Calm;
    return MusicalState::Groove;
}

std::vector<std::string> MusicDirectorScene::preferred(MusicalState state) const {
    switch (state) {
        case MusicalState::Calm:
            return {"audio_aurora", "starfield", "metablob", "reaction_diffusion"};
        case MusicalState::Build:
            return {"boids", "wave_pattern", "neontunnel", "audio_pulse_tunnel"};
        case MusicalState::Peak:
            return {"audio_pulse_tunnel", "audio_kaleidoscope", "audio_particles", "neontunnel"};
        case MusicalState::Groove:
        default:
            return {"boids", "audio_kaleidoscope", "metablob", "audio_particles", "wave_pattern"};
    }
}

bool MusicDirectorScene::child_allowed(const std::string &name) const {
    if (name.empty() || name == get_name()) return false;
    const auto &pool = scene_pool_->get();
    if (!pool.empty() && std::find(pool.begin(), pool.end(), name) == pool.end()) return false;

    for (const auto &wrapper : Plugins::PluginManager::instance()->get_scenes()) {
        if (!wrapper || wrapper->get_name() != name) continue;
        const auto scene = wrapper->get_default();
        if (!scene) return false;
        const auto caps = scene->get_capabilities();
        // The outer active scene remains music_director, so desktop-dependent
        // scenes that require their own plugin packets cannot be nested safely.
        if (caps.requires_desktop && !caps.requires_audio) return false;
        return caps.music_director_eligible && !caps.interactive;
    }
    return false;
}

void MusicDirectorScene::stop_child() noexcept {
    if (!child_) return;
    try { child_->after_render_stop(); }
    catch (const std::exception &e) { spdlog::warn("Music Director child '{}' cleanup failed: {}", child_name_, e.what()); }
    catch (...) { spdlog::warn("Music Director child '{}' cleanup failed with unknown exception", child_name_); }
    child_.reset();
    child_name_.clear();
}

bool MusicDirectorScene::switch_child(MusicalState state) {
    auto candidates = preferred(state);
    const auto &pool = scene_pool_->get();
    for (const auto &name : pool)
        if (std::find(candidates.begin(), candidates.end(), name) == candidates.end()) candidates.push_back(name);
    if (candidates.empty()) return false;

    auto wrappers = Plugins::PluginManager::instance()->get_scenes();
    for (size_t attempt = 0; attempt < candidates.size(); ++attempt) {
        const size_t idx = (selection_cursor_ + attempt) % candidates.size();
        const std::string &name = candidates[idx];
        if (name == child_name_ || !child_allowed(name)) continue;

        const auto wrapper = std::find_if(wrappers.begin(), wrappers.end(), [&](const auto &item) {
            return item && item->get_name() == name;
        });
        if (wrapper == wrappers.end()) continue;

        try {
            auto next = (*wrapper)->create();
            if (!next) continue;
            next->update_default_properties();
            next->register_properties();

            nlohmann::json arguments = nlohmann::json::object();
            if (configure_child_audio_->get()) {
                for (const auto &property : next->get_properties()) {
                    if (!property) continue;
                    if (property->getName() == "audio_reactive") arguments["audio_reactive"] = true;
                    if (property->getName() == "audio_strength") arguments["audio_strength"] = child_audio_strength_->get();
                }
            }
            next->load_properties(arguments);
            next->initialize(matrix_width, matrix_height);

            stop_child();
            child_ = std::move(next);
            child_name_ = name;
            child_state_ = state;
            switched_at_ = frame_context().elapsed_seconds;
            pending_switch_ = false;
            selection_cursor_ = (idx + 1) % candidates.size();
            spdlog::info("Music Director selected '{}'", child_name_);
            return true;
        } catch (const std::exception &e) {
            spdlog::warn("Music Director could not start '{}': {}", name, e.what());
        }
    }
    return false;
}

bool MusicDirectorScene::request_switch(const AudioState::Snapshot &audio, MusicalState state) {
    const double dwell_ms = (frame_context().elapsed_seconds - switched_at_) * 1000.0;
    const bool drop = react_on_drops_->get() && audio.drop_counter != seen_drop_;
    const bool section = react_on_sections_->get() && audio.section_counter != seen_section_;
    const bool maxed = dwell_ms >= static_cast<double>(maximum_dwell_->get());
    const bool changed_energy = state != child_state_ && dwell_ms >= static_cast<double>(minimum_dwell_->get());

    seen_drop_ = audio.drop_counter;
    seen_section_ = audio.section_counter;

    if (!child_) {
        pending_switch_ = true;
        pending_state_ = state;
    } else if (drop && dwell_ms >= std::min<double>(minimum_dwell_->get(), 3500.0)) {
        pending_switch_ = true;
        pending_state_ = MusicalState::Peak;
    } else if ((section || maxed || changed_energy) && dwell_ms >= static_cast<double>(minimum_dwell_->get())) {
        pending_switch_ = true;
        pending_state_ = state;
    }

    if (!pending_switch_) return false;

    const bool confidentTempo = feature(audio, AudioProtocol::Feature::BeatConfidence) >= 0.52f &&
                                feature(audio, AudioProtocol::Feature::TempoStability) >= 0.45f;
    const bool newBeat = audio.beat_counter != seen_beat_;
    seen_beat_ = audio.beat_counter;
    if (beat_sync_->get() && confidentTempo && child_ && !newBeat && !drop) return false;
    return switch_child(pending_state_);
}

bool MusicDirectorScene::render(rgb_matrix::FrameCanvas *canvas) {
    const auto audio = AudioState::snapshot();
    if (!audio.fresh()) {
        canvas->Clear();
        return true;
    }

    const auto state = classify(audio);
    request_switch(audio, state);
    if (!child_ && !switch_child(state)) {
        canvas->Clear();
        return true;
    }

    try {
        const bool keep = child_->render_frame(canvas, frame_context().delta_seconds, true);
        if (!keep) {
            pending_switch_ = true;
            pending_state_ = state;
        }
    } catch (const std::exception &e) {
        Diagnostics::RuntimeDiagnostics::instance().record_scene_error("music_director/" + child_name_, e.what());
        spdlog::error("Music Director child '{}' render failed: {}", child_name_, e.what());
        stop_child();
        pending_switch_ = true;
        pending_state_ = state;
        canvas->Clear();
    } catch (...) {
        Diagnostics::RuntimeDiagnostics::instance().record_scene_error("music_director/" + child_name_, "unknown exception");
        spdlog::error("Music Director child '{}' render failed with unknown exception", child_name_);
        stop_child();
        pending_switch_ = true;
        pending_state_ = state;
        canvas->Clear();
    }
    return true;
}

void MusicDirectorScene::after_render_stop() { stop_child(); }
void MusicDirectorScene::before_transition_stop() {
    if (!child_) return;
    try { child_->before_transition_stop(); } catch (...) {}
}

} // namespace Scenes
