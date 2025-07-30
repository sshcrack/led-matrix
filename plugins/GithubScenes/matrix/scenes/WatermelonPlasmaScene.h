#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"

using Scenes::Scene;
namespace Scenes {
    class WatermelonPlasmaScene : public Scene {
    private:
        FrameTimer frameTimer;
    public:
        ~WatermelonPlasmaScene() override = default;
        bool render(RGBMatrixBase *matrix) override;

        string get_name() const override;

        void register_properties() override {}

        using Scenes::Scene::Scene;

        tmillis_t get_default_duration() override {
            return 10000;
        }

        int get_default_weight() override {
            return 1;
        }
    };

    class WatermelonPlasmaSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}