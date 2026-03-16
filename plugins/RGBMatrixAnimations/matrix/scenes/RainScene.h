#pragma once

#include "ParticleScene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "../anim/gravityparticles.h"
#include <led-matrix.h>  // Add direct include for rgb_matrix types

namespace Scenes {
    class RainScene : public ParticleScene {
    private:
        uint16_t *cols;
        uint16_t *vels;
        uint8_t *lengths;
        uint16_t totalCols;
        uint16_t currentColorId;
        uint16_t totalColors;
        uint32_t counter;


        void initializeParticles(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation) override;

        void initializeColumns();

        void createColorPalette(std::shared_ptr<ParticleMatrixRenderer> renderer);

        void addNewParticles(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation);

        void removeOldParticles(std::shared_ptr<GravityParticles> animation);
    protected:
        void preRender(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation) override;

    public:
        explicit RainScene();

        ~RainScene() override;

        bool render(rgb_matrix::FrameCanvas *canvas) override;

        void initialize(int width, int height) override; // Add override here instead
        [[nodiscard]] string get_name() const override;

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 2;
        }
    };

    class RainSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scene, void (*)(Scene *)> create() override;
    };
}
