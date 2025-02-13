#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include "utils/neuralNetwork.hpp"
#include "utils/grid.hpp"
#include "utils/brain.hpp"
#include "utils/piece.hpp"
#include <vector>
#include <chrono>

namespace Scenes {
    class TetrisScene : public Scene {
    private:
        Grid grid;
        Brain brain;
        NeuralNetwork bestParams;
        bool gameOver = false;
        bool fixed = false;
        bool rotated = false;
        std::string bestMove;
        int bestRotation = 0;

        struct RGB {
            uint8_t r, g, b;
        };

        // Replace Color array with RGB values
        const RGB colorList[8] = {
                {255, 255, 0},   // YELLOW
                {255, 165, 0},   // ORANGE
                {148, 0,   211},   // VIOLET
                {255, 0,   0},     // RED
                {0,   255, 0},     // GREEN
                {255, 192, 203}, // PINK
                {0,   0,   139},      // DARKBLUE
                {255, 255, 255}      // DARKBLUE
        };

        int offsets[7][2] = {{35,  -30},
                             {5,   15},
                             {20,  0},
                             {-10, 0},
                             {15,  -15},
                             {20,  0},
                             {20,  5}};
        int pixel_width = 32;
        int pixel_height = 64;
        int block_size = 3; // Size of each Tetris block
        int offset_x = 0;   // Horizontal centering offset
        int offset_y = 0;   // Vertical centering offset

        PropertyPointer<int> fall_speed_ms = MAKE_PROPERTY("fall_speed_ms", int, 100);
        std::chrono::steady_clock::time_point last_fall_time;

    public:
        explicit TetrisScene();

        bool render(rgb_matrix::RGBMatrix *matrix) override;

        void initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        void after_render_stop(rgb_matrix::RGBMatrix *matrix) override;

        [[nodiscard]] std::string get_name() const override;

        void register_properties() override {
            add_property(fall_speed_ms);
        }

        using Scene::Scene;
    };

    class TetrisSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
