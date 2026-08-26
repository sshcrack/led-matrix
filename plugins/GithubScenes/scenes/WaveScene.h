#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include <random>
#include <vector>

using namespace Scenes;

namespace Scenes {
    class WaveScene : public Scene {
    private:
        std::vector<float> map_;
        std::vector<float> next_map_;
        std::mt19937 rng{std::random_device{}()};

        void drawMap(rgb_matrix::FrameCanvas *canvas, const std::vector<float> &map) const;

    public:
        bool render(rgb_matrix::FrameCanvas *canvas) override;

        using Scene::Scene::Scene;
        ~WaveScene() override = default;

        void initialize(int width, int height) override;

        [[nodiscard]] string get_name() const override;
        std::string get_category() const override { return "Generative"; }
        [[nodiscard]] SceneDescriptor get_descriptor() const override;
        [[nodiscard]] bool supports_virtual_time() const override { return true; }

        void register_properties() override {}


        tmillis_t get_default_duration() override {
            return 10000;
        }

        int get_default_weight() override {
            return 1;
        }
    };


    class WaveSceneWrapper : public Plugins::SceneWrapper {
    public:

        std::unique_ptr<Scenes::Scene> create() override;
    };
}