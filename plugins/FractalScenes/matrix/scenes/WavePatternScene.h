#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"

namespace Scenes {

class WavePatternSceneWrapper final : public Plugins::SceneWrapper {
public:
    std::unique_ptr<Scene> create() override;
};

class WavePatternScene final : public Scene {
public:
    WavePatternScene();
    ~WavePatternScene() override = default;

    void initialize(int width, int height) override;
    bool render(rgb_matrix::FrameCanvas* canvas) override;
    string get_name() const override;
    std::string get_category() const override { return "Fractals"; }
    SceneCapabilities get_capabilities() const override
    {
        auto caps = Scene::get_capabilities();
        caps.supports_audio = true;
        return caps;
    }

    [[nodiscard]] Previews::SceneSpec get_preview_spec() const override {
        auto spec = Previews::SceneSpec::with_inputs({Previews::Inputs::Audio});
        spec.set_property("audio_reactive", true).set_property("audio_strength", 1.0f);
        return spec;
    }
    [[nodiscard]] bool supports_virtual_time() const override { return true; }
    tmillis_t get_default_duration() override { return 30000; }
    int get_default_weight() override { return 2; }

protected:
    void register_properties() override;
    void load_properties(const json& j) override;

private:
    struct Wave {
        float amplitude;
        float frequency;
        float phase_speed;
        float phase_offset;
    };

    std::vector<Wave> waves_;
    std::mt19937 rng_{std::random_device{}()};
    float total_time_ = 0.0f;
    float audio_bass_ = 0.0f;
    float audio_mids_ = 0.0f;
    float audio_treble_ = 0.0f;
    float beat_pulse_ = 0.0f;
    float drop_pulse_ = 0.0f;
    std::uint64_t last_beat_counter_ = 0;
    std::uint64_t last_drop_counter_ = 0;

    PropertyPointer<int> num_waves_ = MAKE_PROPERTY_MINMAX("num_waves", int, 4, 1, 10);
    PropertyPointer<float> speed_ = MAKE_PROPERTY_MINMAX("speed", float, 0.8f, 0.1f, 5.0f);
    PropertyPointer<float> color_speed_ = MAKE_PROPERTY_MINMAX("color_speed", float, 0.5f, 0.0f, 2.0f);
    PropertyPointer<bool> rainbow_mode_ = MAKE_PROPERTY("rainbow_mode", bool, true);
    PropertyPointer<float> wave_height_ = MAKE_PROPERTY_MINMAX("wave_height", float, 0.9f, 0.1f, 2.0f);
    PropertyPointer<int> ribbon_layers_ = MAKE_PROPERTY_MINMAX("ribbon_layers", int, 5, 1, 9);
    PropertyPointer<int> glow_radius_ = MAKE_PROPERTY_MINMAX("glow_radius", int, 2, 0, 5);
    PropertyPointer<bool> audio_reactive_ = MAKE_PROPERTY("audio_reactive", bool, false);
    PropertyPointer<float> audio_strength_ = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.85f, 0.0f, 2.0f);

    void init_waves();
    void update_audio(float dt);
    [[nodiscard]] float sample_wave(float normalized_x, float layer_phase) const;
    static void add_pixel(rgb_matrix::FrameCanvas* canvas, int x, int y, uint8_t r, uint8_t g, uint8_t b);
};

}  // namespace Scenes
