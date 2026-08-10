#pragma once

#include <memory>
#include <string>
#include <vector>

#include <shared/matrix/Scene.h>
#include <shared/matrix/audio_state.h>
#include <shared/matrix/wrappers.h>

namespace Scenes {

class MusicDirectorScene final : public Scene {
    enum class MusicalState { Calm, Groove, Build, Peak };
    enum class BeatQuantization { BEAT = 1, TWO_BEATS = 2, BAR = 4, TWO_BARS = 8 };

    PropertyPointer<std::vector<std::string>> scene_pool_ = MAKE_STRING_LIST_PROPERTY(
        "scene_pool", std::vector<std::string>({
            "audio_aurora", "starfield", "metablob", "boids", "wave_pattern",
            "reaction_diffusion", "neontunnel", "audio_pulse_tunnel",
            "audio_kaleidoscope", "audio_particles"}));
    PropertyPointer<tmillis_t> minimum_dwell_ = MAKE_PROPERTY_MINMAX("minimum_dwell", tmillis_t, 9000, 2000, 60000);
    PropertyPointer<tmillis_t> maximum_dwell_ = MAKE_PROPERTY_MINMAX("maximum_dwell", tmillis_t, 26000, 6000, 120000);
    PropertyPointer<bool> beat_sync_ = MAKE_PROPERTY("beat_sync", bool, true);
    PropertyPointer<Plugins::EnumProperty<BeatQuantization>> beat_quantization_ = MAKE_ENUM_PROPERTY("beat_quantization", BeatQuantization, BeatQuantization::BAR);
    PropertyPointer<bool> react_on_sections_ = MAKE_PROPERTY("react_on_sections", bool, true);
    PropertyPointer<bool> react_on_drops_ = MAKE_PROPERTY("react_on_drops", bool, true);
    PropertyPointer<bool> configure_child_audio_ = MAKE_PROPERTY("configure_child_audio", bool, true);
    PropertyPointer<float> child_audio_strength_ = MAKE_PROPERTY_MINMAX("child_audio_strength", float, 1.0f, 0.0f, 3.0f);
    PropertyPointer<bool> switch_effects_ = MAKE_PROPERTY("switch_effects", bool, true);
    PropertyPointer<bool> spotify_artwork_colors_ = MAKE_PROPERTY("spotify_artwork_colors", bool, true);

    std::unique_ptr<Scene> child_;
    std::string child_name_;
    MusicalState child_state_ = MusicalState::Calm;
    double switched_at_ = 0.0;
    bool pending_switch_ = false;
    MusicalState pending_state_ = MusicalState::Calm;
    uint64_t seen_beat_ = 0;
    uint64_t seen_drop_ = 0;
    uint64_t seen_section_ = 0;
    size_t selection_cursor_ = 0;

    MusicalState classify(const AudioState::Snapshot &audio) const;
    bool request_switch(const AudioState::Snapshot &audio, MusicalState state);
    bool switch_child(MusicalState state);
    std::vector<std::string> preferred(MusicalState state) const;
    bool child_allowed(const std::string &name) const;
    void stop_child() noexcept;

public:
    MusicDirectorScene() = default;
    ~MusicDirectorScene() override { stop_child(); }

    bool render(rgb_matrix::FrameCanvas *canvas) override;
    void initialize(int width, int height) override;
    void register_properties() override;
    void after_render_stop() override;
    void before_transition_stop() override;

    std::string get_name() const override { return "music_director"; }
    std::string get_category() const override { return "Audio Reactive"; }
    tmillis_t get_default_duration() override { return 120000; }
    int get_default_weight() override { return 7; }
    bool needs_desktop_app() override { return true; }
    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        return Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
    }
    SceneCapabilities get_capabilities() const override {
        auto caps = Scene::get_capabilities();
        caps.requires_audio = true;
        caps.supports_audio = true;
        caps.music_director_eligible = false;
        return caps;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
};

class MusicDirectorSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<MusicDirectorScene>(); }
};

} // namespace Scenes
