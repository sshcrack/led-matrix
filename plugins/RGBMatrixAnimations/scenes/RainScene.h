#pragma once

#include "ParticleScene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"
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


        void initializeParticles() override;

        void initializeColumns();

        void createColorPalette();

        void addNewParticles();

        void removeOldParticles();

    public:
        explicit RainScene();

        ~RainScene() override;

        bool render(RGBMatrixBase *matrix) override;

        void initialize(RGBMatrixBase *p_matrix, FrameCanvas *l_offscreen_canvas) override; // Add override here instead
        [[nodiscard]] string get_name() const override;

        void after_render_stop(RGBMatrixBase *matrix) override {
        }
    };

    class RainSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scene, void (*)(Scene *)> create() override;
    };
}
