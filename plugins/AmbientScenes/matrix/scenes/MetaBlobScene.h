#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>
#include <chrono>

namespace AmbientScenes {
    class MetaBlobScene : public Scenes::Scene {
    private:
        PropertyPointer<int> num_blobs = MAKE_PROPERTY("num_blobs", int, 10);
        PropertyPointer<float> threshold = MAKE_PROPERTY("threshold", float, 0.0003f);
        PropertyPointer<float> speed = MAKE_PROPERTY("speed", float, 0.25f);
        PropertyPointer<float> move_range = MAKE_PROPERTY("move_range", float, 0.5f);
        PropertyPointer<bool> audio_reactive = MAKE_PROPERTY("audio_reactive", bool, false);
        PropertyPointer<float> audio_strength = MAKE_PROPERTY_MINMAX("audio_strength", float, 0.75f, 0.0f, 2.0f);
        PropertyPointer<float> color_speed = MAKE_PROPERTY("color_speed", float, 0.033f);

        float time;
        std::chrono::steady_clock::time_point last_update;
        float audio_bass = 0.0f, audio_mids = 0.0f, audio_treble = 0.0f;
        uint64_t last_beat_counter = 0;

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

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 1;
        }

        [[nodiscard]] std::string get_name() const override;

        void register_properties() override;

        using Scene::Scene;
    };

    class MetaBlobSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
