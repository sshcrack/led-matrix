#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"
#include "../anim/gravityparticles.h"
#include <led-matrix.h>

namespace Scenes {
    class ParticleMatrixRenderer : public RGBMatrixRenderer {
    public:
        ParticleMatrixRenderer(uint16_t width, uint16_t height, rgb_matrix::Canvas* canvas)
            : RGBMatrixRenderer(width, height), canvas_(canvas) {}
        
        void setPixel(uint16_t x, uint16_t y, RGB_color colour) override {
            if (canvas_) {
                canvas_->SetPixel(x, gridHeight - y - 1, colour.r, colour.g, colour.b);
            }
        }

        void showPixels() override {
            // Nothing to do - pixels are shown immediately on the matrix
        }

        void outputMessage(char msg[]) override {
            fprintf(stderr, msg);
        }

        void msSleep(int ms) override {
            usleep(ms * 1000);
        }

        int16_t random_int16(int16_t a, int16_t b) override {
            return a + rand()%(b-a);
        }
    protected:
        rgb_matrix::Canvas* canvas_;
    };

    class ParticleScene : public Scene {
    protected:
        rgb_matrix::Canvas* matrix;
        ParticleMatrixRenderer* renderer;
        GravityParticles* animation;

        Property<int> numParticles = Property("numParticles", 40);
        Property<int16_t> velocity = Property<int16_t>("velocity", 6000);
        Property<int> accel = Property("acceleration", 1);
        Property<int> shake = Property("shake", 5);
        Property<int> bounce = Property("bounce", 250);
        Property<int> delay_ms = Property("delay_ms", 10);
        
        uint64_t prevTime;
        uint64_t lastFpsLog;
        uint32_t frameCount;

        static uint64_t micros();
        int16_t random_int16(int16_t a, int16_t b) {  // Added helper function
            return renderer ? renderer->random_int16(a, b) : a + rand() % (b - a);
        }

        virtual void initializeParticles() = 0;

    public:
        explicit ParticleScene();
        ~ParticleScene() override;

        void register_properties() override;

        bool render(RGBMatrix *rgbMatrix) override;
        void initialize(rgb_matrix::RGBMatrix *p_matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;
        void after_render_stop(rgb_matrix::RGBMatrix *m) override;
    };
}
