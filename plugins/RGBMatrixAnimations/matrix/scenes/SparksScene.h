#pragma once

#include "ParticleScene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "../anim/gravityparticles.h"
#include <led-matrix.h>

namespace Scenes {
    class SparksScene : public ParticleScene {
        int16_t ax, ay;

        void initializeParticles(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation) override;

    public:
        explicit SparksScene();

        ~SparksScene() override = default;

        [[nodiscard]] string get_name() const override;


        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 2;
        }

        void particle_on_render_stop(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation) override;
    };

    class SparksSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
