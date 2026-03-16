#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"

using namespace Scenes;

namespace Scenes {
    class WaveScene : public Scene {
    private:
        float *map = nullptr;
        FrameTimer frameTimer;

        void drawMap(rgb_matrix::FrameCanvas *canvas, float *iMap);

    public:
        bool render(rgb_matrix::FrameCanvas *canvas) override;

        using Scene::Scene::Scene;
        ~WaveScene() override;

        void initialize(int width, int height) override;

        [[nodiscard]] string get_name() const override;

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

        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}