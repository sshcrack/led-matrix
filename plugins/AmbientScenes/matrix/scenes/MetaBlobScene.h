#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
    class MetaBlobScene : public Scenes::Scene {
    private:
        PropertyPointer<int> num_blobs = MAKE_PROPERTY_MINMAX("num_blobs", int, 10, 1, 24);
        PropertyPointer<float> threshold = MAKE_PROPERTY_MINMAX("threshold", float, 0.0003f, 0.00003f, 0.0012f);
        PropertyPointer<float> speed = MAKE_PROPERTY_MINMAX("speed", float, 0.25f, 0.03f, 1.5f);
        PropertyPointer<float> move_range = MAKE_PROPERTY_MINMAX("move_range", float, 0.5f, 0.05f, 1.0f);
        PropertyPointer<bool> audio_reactive = MAKE_PROPERTY("audio_reactive", bool, false);
        PropertyPointer<float> audio_strength = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.75f, 0.0f, 2.0f);
        PropertyPointer<float> color_speed = MAKE_PROPERTY_MINMAX("color_speed", float, 0.033f, 0.0f, 0.25f);

        float time;
        float audio_bass = 0.0f, audio_mids = 0.0f, audio_treble = 0.0f, audio_balance = 0.0f;
        float beat_pulse = 0.0f, drop_pulse = 0.0f, section_hue = 0.0f;
        uint64_t last_beat_counter = 0, last_drop_counter = 0, last_section_counter = 0;

        struct Blob {
            float x, y;
            float radius;

            Blob(float x, float y, float radius) : x(x), y(y), radius(radius) {
            }
        };

        std::vector<Blob> blobs;

        // Shader conversion helpers
        float rand_sin(int i) const;

        Blob get_blob(int i, float time) const;

        float calculate_field(float x, float y, const Blob &blob) const;

    public:
        explicit MetaBlobScene();

        ~MetaBlobScene() override = default;

        bool render(rgb_matrix::FrameCanvas *canvas) override;

        void initialize(int width, int height) override;

        [[nodiscard]] bool supports_virtual_time() const override { return true; }
        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 1;
        }

        [[nodiscard]] std::string get_name() const override;
        [[nodiscard]] std::string get_category() const override { return "Ambient"; }

        Scenes::SceneCapabilities get_capabilities() const override { auto caps = Scenes::Scene::get_capabilities(); caps.supports_audio = true; return caps; }
        void register_properties() override;

        using Scene::Scene;
    };

    class MetaBlobSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
