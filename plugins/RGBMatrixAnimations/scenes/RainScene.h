#pragma once
#include <shared/matrix/utils/AmbiancePalette.h>

#include "ParticleScene.h"
#include "shared/matrix/wrappers.h"
#include "matrix/anim/gravityparticles.h"
#include <led-matrix.h>  // Add direct include for rgb_matrix types

namespace Scenes {
    class RainScene : public ParticleScene {
    private:
    PropertyPointer<Plugins::EnumProperty<Ambiance::Palette>> look_palette_ =
        MAKE_ENUM_PROPERTY("look_palette", Ambiance::Palette, Ambiance::Palette::ORIGINAL);
    PropertyPointer<float> look_brightness_ = MAKE_PROPERTY_MINMAX("look_brightness", float, 1.0f, 0.05f, 1.0f);
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

        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;

        void initialize(int width, int height) override; // Add override here instead
        [[nodiscard]] string get_name() const override;
        [[nodiscard]] SceneDescriptor get_descriptor() const override;

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 2;
        }
    };

    class RainSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scene> create() override;
    };
}
