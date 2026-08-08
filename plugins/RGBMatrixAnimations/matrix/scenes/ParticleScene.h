#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "../anim/gravityparticles.h"
#include "spdlog/spdlog.h"
#include <led-matrix.h>
#include <random>

namespace Scenes {
    class ParticleMatrixRenderer : public RGBMatrixRenderer {
    public:
        ParticleMatrixRenderer(uint16_t width, uint16_t height, rgb_matrix::Canvas *canvas)
            : RGBMatrixRenderer(width, height), canvas_(canvas) {
        }

        void setCanvas(rgb_matrix::Canvas *canvas) {
            if (canvas_ == canvas) {
                return;
            }

            canvas_ = canvas;
            updateDisplay();
        }

        void setPixel(uint16_t x, uint16_t y, RGB_color colour) override {
            if (canvas_) {
                canvas_->SetPixel(x, gridHeight - y - 1, colour.r, colour.g, colour.b);
            }
        }

        void showPixels() override {
            // Nothing to do - pixels are shown immediately on the matrix
        }

        void msSleep(int ms) override {
            usleep(ms * 1000);
        }

        int16_t random_int16(int16_t a, int16_t b) override {
            std::uniform_int_distribution<int16_t> dist(a, b - 1);
            return dist(rng);
        }

    protected:
        rgb_matrix::Canvas *canvas_;
        std::mt19937 rng{std::random_device{}()};
    };

    class ParticleScene : public Scene {
    private:
        std::optional<std::shared_ptr<ParticleMatrixRenderer> > renderer;
        std::optional<std::shared_ptr<GravityParticles> > animation;
        FixedStepAccumulator simulation_{100.0, 8};

        void after_render_stop() override;
    protected:
        PropertyPointer<int> num_particles = MAKE_PROPERTY_MINMAX("num_particles", int, 40, 1, 12000);
        PropertyPointer<int16_t> velocity = MAKE_PROPERTY_MINMAX("velocity", int16_t, 6000, 100, 16000);
        PropertyPointer<int> accel = MAKE_PROPERTY_MINMAX("acceleration", int, 1, 0, 100);
        PropertyPointer<int> shake = MAKE_PROPERTY_MINMAX("shake", int, 5, 0, 100);
        PropertyPointer<int> bounce = MAKE_PROPERTY_MINMAX("bounce", int, 250, 0, 255);
        PropertyPointer<int> delay_ms = MAKE_PROPERTY_MINMAX("delay_ms", int, 10, 4, 50);

        std::mt19937 scene_rng{std::random_device{}()};

        int16_t random_int16(int16_t a, int16_t b) {
            // Added helper function
            std::uniform_int_distribution<int16_t> dist(a, b - 1);
            return renderer ? renderer->get()->random_int16(a, b) : dist(scene_rng);
        }

        virtual void initializeParticles(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation) = 0;
        virtual void preRender(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation)
        {

        }

    public:
        explicit ParticleScene();

        ~ParticleScene() override = default;

        void register_properties() override;

        bool render(rgb_matrix::FrameCanvas *canvas) override;

        void initialize(int width, int height) override;

        std::string get_category() const override { return "Particles"; }
        [[nodiscard]] bool supports_virtual_time() const override { return true; }

        virtual void particle_on_render_stop(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation)
        {

        }
    };
}
