#pragma once

#include "ParticleScene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"
#include "../anim/gravityparticles.h"
#include <led-matrix.h>

namespace Scenes {
    class SparksScene : public ParticleScene {
    private:
        int16_t ax, ay;
        void initializeParticles() override;

    public:
        explicit SparksScene(const nlohmann::json &config);
        ~SparksScene() override;
        [[nodiscard]] string get_name() const override;
        void initialize(RGBMatrix *p_matrix) override;
    };

    class SparksSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;
        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
