#pragma once

#include "Scene.h"
#include "wrappers.h"
#include "shared/utils/FrameTimer.h"
#include "../anim/gravityparticles.h"
#include <led-matrix.h>  // Add direct include for rgb_matrix types

namespace Scenes {
    class RainMatrixRenderer : public RGBMatrixRenderer {
    public:
        RainMatrixRenderer(uint16_t width, uint16_t height, rgb_matrix::Canvas* canvas)
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
    private:
        rgb_matrix::Canvas* canvas_;
    };

    class RainScene : public Scene {
    private:
        rgb_matrix::Canvas* matrix;

        RainMatrixRenderer* renderer;
        GravityParticles *animation;
        uint16_t numParticles;
        int16_t velocity;
        int16_t accel;
        uint16_t shake;
        uint8_t bounce;
        
        // New members for rain animation
        uint16_t *cols;
        uint16_t *vels;
        uint8_t *lengths;
        uint16_t totalCols;
        uint16_t currentColorId;
        uint16_t totalColors;
        uint32_t counter;
        
        uint64_t prevTime;
        uint64_t lastFpsLog;  // Track last FPS log time
        uint32_t frameCount;  // Count frames between logs
        int delay_ms;
        
        uint64_t micros(); // Get time stamp in microseconds

        void initializeColumns();
        void createColorPalette();
        void addNewParticles();
        void removeOldParticles();

        int16_t random_int16(int16_t a, int16_t b);
    public:
        explicit RainScene(const nlohmann::json &config);
        ~RainScene();

        bool render(RGBMatrix *matrix) override;
        [[nodiscard]] string get_name() const override;
        void initialize(RGBMatrix *p_matrix) override;
    };

    class RainSceneWrapper : public Plugins::SceneWrapper {
        Scenes::Scene *create_default() override;
        Scenes::Scene *from_json(const nlohmann::json &args) override;
    };
}
