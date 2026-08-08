#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
    class StarFieldScene : public Scenes::Scene {
    private:
        struct Star {
            float x = 0.0f, y = 0.0f, z = 1.0f;
            float previous_z = 1.0f;
            float hue = 0.0f;
            void respawn(float max_depth, std::mt19937& rng);
            void update(float speed);
        };

        std::vector<Star> stars;
        std::mt19937 gen{std::random_device{}()};
        float time = 0.0f;
        float audio_bass = 0.0f, audio_mids = 0.0f, audio_treble = 0.0f;
        uint64_t last_beat_counter = 0;
        float beat_flash = 0.0f;

        PropertyPointer<int> num_stars = MAKE_PROPERTY_MINMAX("num_stars", int, 90, 16, 240);
        PropertyPointer<float> speed = MAKE_PROPERTY_MINMAX("speed", float, 0.025f, 0.003f, 0.12f);
        PropertyPointer<bool> enable_twinkle = MAKE_PROPERTY("enable_twinkle", bool, true);
        PropertyPointer<float> max_depth = MAKE_PROPERTY_MINMAX("max_depth", float, 3.0f, 0.5f, 6.0f);
        PropertyPointer<bool> colored_stars = MAKE_PROPERTY("colored_stars", bool, true);
        PropertyPointer<float> streak_length = MAKE_PROPERTY_MINMAX("streak_length", float, 0.7f, 0.0f, 2.0f);
        PropertyPointer<bool> audio_reactive = MAKE_PROPERTY("audio_reactive", bool, false);
        PropertyPointer<float> audio_strength = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.8f, 0.0f, 2.0f);
        PropertyPointer<bool> drifting_center = MAKE_PROPERTY("drifting_center", bool, true);

        static void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);
        static void draw_line(rgb_matrix::FrameCanvas* canvas, int x0, int y0, int x1, int y1,
                              uint8_t r, uint8_t g, uint8_t b, int width, int height);

    public:
        explicit StarFieldScene();
        ~StarFieldScene() override = default;
        Scenes::SceneCapabilities get_capabilities() const override { auto caps = Scenes::Scene::get_capabilities(); caps.supports_audio = true; return caps; }
        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;
        [[nodiscard]] bool supports_virtual_time() const override { return true; }
        tmillis_t get_default_duration() override { return 20000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;
        [[nodiscard]] std::string get_category() const override { return "Ambient"; }
        using Scene::Scene;
    };

    class StarFieldSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
