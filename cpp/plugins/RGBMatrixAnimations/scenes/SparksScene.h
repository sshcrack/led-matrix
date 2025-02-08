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
        explicit SparksScene();
        ~SparksScene() override;
        [[nodiscard]] string get_name() const override;
        void initialize(RGBMatrix *p_matrix) override;

        void after_render_stop(rgb_matrix::RGBMatrix *matrix) override;
    };

    class SparksSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create() override;
    };
}
