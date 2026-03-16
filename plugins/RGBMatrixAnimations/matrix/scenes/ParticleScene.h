#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "../anim/gravityparticles.h"
#include "spdlog/spdlog.h"
#include <led-matrix.h>

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
            return a + rand() % (b - a);
        }

    protected:
        rgb_matrix::Canvas *canvas_;
    };

    class ParticleScene : public Scene {
    private:
        std::optional<std::shared_ptr<ParticleMatrixRenderer> > renderer;
        std::optional<std::shared_ptr<GravityParticles> > animation;

        void after_render_stop() override;
    protected:
        PropertyPointer<int> numParticles = MAKE_PROPERTY("numParticles", int, 40);
        PropertyPointer<int16_t> velocity = MAKE_PROPERTY("velocity", int16_t, 6000);
        PropertyPointer<int> accel = MAKE_PROPERTY("acceleration", int, 1);
        PropertyPointer<int> shake = MAKE_PROPERTY("shake", int, 5);
        PropertyPointer<int> bounce = MAKE_PROPERTY("bounce", int, 250);
        PropertyPointer<int> delay_ms = MAKE_PROPERTY("delay_ms", int, 10);

        uint64_t prevTime;
        uint64_t lastFpsLog;
        uint32_t frameCount;

        static uint64_t micros();

        int16_t random_int16(int16_t a, int16_t b) {
            // Added helper function
            return renderer ? renderer->get()->random_int16(a, b) : a + rand() % (b - a);
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

        virtual void particle_on_render_stop(std::shared_ptr<ParticleMatrixRenderer> renderer, std::shared_ptr<GravityParticles> animation)
        {

        }
    };
}
