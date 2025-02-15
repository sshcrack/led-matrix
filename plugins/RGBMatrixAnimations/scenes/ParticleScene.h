#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "../anim/gravityparticles.h"
#include <spdlog/spdlog.h>
#include <led-matrix.h>

namespace Scenes {
    class ParticleMatrixRenderer : public RGBMatrixRenderer {
    public:
        ParticleMatrixRenderer(uint16_t width, uint16_t height, rgb_matrix::Canvas *canvas)
            : RGBMatrixRenderer(width, height), canvas_(canvas) {
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
    protected:
        rgb_matrix::Canvas *matrix;
        std::optional<std::shared_ptr<ParticleMatrixRenderer> > renderer;
        std::optional<std::unique_ptr<GravityParticles, void(*)(GravityParticles *)> > animation;

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

        virtual void initializeParticles() = 0;

    public:
        explicit ParticleScene();

        ~ParticleScene() override = default;

        void register_properties() override;

        bool render(RGBMatrix *rgbMatrix) override;

        void initialize(rgb_matrix::RGBMatrix *p_matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        void after_render_stop(rgb_matrix::RGBMatrix *m) override;
    };
}
