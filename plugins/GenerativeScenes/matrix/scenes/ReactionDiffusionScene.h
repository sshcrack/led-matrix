#pragma once

#include <cstdint>
#include <random>
#include <tuple>
#include <vector>

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace GenerativeScenes {

// Gray-Scott reaction-diffusion model. Two virtual chemicals interact and
// diffuse into spots, labyrinths, stripes and coral-like structures.
class ReactionDiffusionScene final : public Scenes::Scene {
public:
    ReactionDiffusionScene();
    ~ReactionDiffusionScene() override = default;

    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    void register_properties() override;

    std::string get_name() const override { return "reaction_diffusion"; }
    std::string get_category() const override { return "Generative"; }
    Scenes::SceneCapabilities get_capabilities() const override
    {
        auto caps = Scenes::Scene::get_capabilities();
        caps.supports_audio = true;
        caps.deterministic_preview = true;
        return caps;
    }
    tmillis_t get_default_duration() override { return 60000; }
    int get_default_weight() override { return 3; }

private:
    struct Preset {
        float feed;
        float kill;
        const char* name;
    };

    static constexpr Preset PRESETS[] = {
        // The old 0.035/0.065 pair decays to a blank field with this
        // discretisation. This spot regime remains self-sustaining, so the
        // scene stays animated instead of going dark until the next cycle.
        {0.0367f, 0.0649f, "spots"}, {0.025f, 0.060f, "mitosis"}, {0.029f, 0.057f, "maze"},
        {0.039f, 0.058f, "bubbles"}, {0.060f, 0.062f, "stripes"},
    };
    static constexpr int NUM_PRESETS = 5;
    static constexpr int STEPS_PER_PRESET = 4000;
    static constexpr int SIM_STEPS_PER_TICK = 8;
    static constexpr float DU = 0.16f;
    static constexpr float DV = 0.08f;
    static constexpr float DT = 1.0f;

    std::vector<float> u_cur_;
    std::vector<float> v_cur_;
    std::vector<float> u_next_;
    std::vector<float> v_next_;
    std::mt19937 rng_{std::random_device{}()};
    Scenes::FixedStepAccumulator simulation_{30.0, 4};

    int current_preset_ = 0;
    int step_count_ = 0;
    float global_hue_ = 0.0f;

    float audio_bass_ = 0.0f;
    float audio_mids_ = 0.0f;
    float audio_treble_ = 0.0f;
    float audio_balance_ = 0.0f;
    float beat_pulse_ = 0.0f;
    float drop_pulse_ = 0.0f;
    std::uint64_t last_beat_counter_ = 0;
    std::uint64_t last_drop_counter_ = 0;

    PropertyPointer<float> simulation_speed_ = MAKE_PROPERTY_MINMAX("simulation_speed", float, 1.0f, 0.35f, 2.0f);
    PropertyPointer<float> color_speed_ = MAKE_PROPERTY_MINMAX("color_speed", float, 1.0f, 0.0f, 3.0f);
    PropertyPointer<float> contrast_ = MAKE_PROPERTY_MINMAX("contrast", float, 1.0f, 0.5f, 2.0f);
    PropertyPointer<bool> audio_reactive_ = MAKE_PROPERTY("audio_reactive", bool, false);
    PropertyPointer<float> audio_strength_ = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.85f, 0.0f, 2.0f);

    [[nodiscard]] int index(int x, int y) const { return y * matrix_width + x; }
    void reset_pattern(bool advance_preset);
    void seed_random_patch();
    void seed_patch(int center_x, int center_y, int radius, float concentration = 1.0f);
    void simulation_step(float feed, float kill);
    void update_audio(float dt);
    void inject_audio_event(bool drop);
    static std::tuple<uint8_t, uint8_t, uint8_t> palette(float value, float hue_shift, float contrast);
};

class ReactionDiffusionSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scenes::Scene> create() override { return std::make_unique<ReactionDiffusionScene>(); }
};

}  // namespace GenerativeScenes
