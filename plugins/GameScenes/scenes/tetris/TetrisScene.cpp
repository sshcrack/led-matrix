#include "TetrisScene.h"
#include <string>
#include "utils/piece.hpp"
#include "utils/grid.hpp"
#include "utils/neuralNetwork.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

using namespace std;

namespace Scenes {
    TetrisScene::TetrisScene() :
            Scene() {
        bestParams = NeuralNetwork(BEST_PARAMS);
        brain = Brain(bestParams);
        last_fall_time = std::chrono::steady_clock::now();
    }

    void TetrisScene::initialize(rgb_matrix::RGBMatrix *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);

        // Calculate scaling and offsets
        block_size = std::min(matrix->width() / 10, matrix->height() / 20);
        offset_x = (matrix->width() - (10 * block_size)) / 2;
        offset_y = (matrix->height() - (20 * block_size)) / 2;

        bestMove = brain.getBestMove(grid);
        bestRotation = (int) bestMove.back() - 48;
        bestMove.pop_back();
    }

    bool TetrisScene::render(rgb_matrix::RGBMatrix *matrix) {
        if (gameOver) return false;

        if (!rotated) {
            for (int i = 0; i < bestRotation; i++) grid.rotatePiece();
            rotated = true;
        }

        if (!bestMove.empty() && !grid.isAnimating) {
            if (bestMove[0] == 'r') {
                grid.movePiece(1, 9);
                bestMove.pop_back();
            } else {
                grid.movePiece(-1, 0);
                bestMove.pop_back();
            }
        }

        if (fixed) {
            bestMove = brain.getBestMove(grid);
            bestRotation = (int) bestMove.back() - 48;
            bestMove.pop_back();
            rotated = false;
            fixed = false;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto time_since_last_fall = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - last_fall_time).count();

        if (time_since_last_fall >= fall_speed_ms.get() && !grid.isAnimating) {
            grid.gravity(1);
            last_fall_time = current_time;
        }

        if (grid.isAnimating) {
            grid.updateAnimation();
        } else {
            grid.clearLine();
            grid.update();
        }

        if (grid.gameOver) {
            gameOver = true;
            return false;
        }


        if (grid.piece.fixed) {
            grid.piece.newShape();
            grid.piece.newNext();
            fixed = true;
        }

        offscreen_canvas->Clear();

        // Draw game outline
        for (int x = offset_x - 1; x <= offset_x + (10 * block_size); x++) {
            offscreen_canvas->SetPixel(x, offset_y - 1, 50, 50, 50);  // top
            offscreen_canvas->SetPixel(x, offset_y + (20 * block_size), 50, 50, 50);  // bottom
        }
        for (int y = offset_y - 1; y <= offset_y + (20 * block_size); y++) {
            offscreen_canvas->SetPixel(offset_x - 1, y, 50, 50, 50);  // left
            offscreen_canvas->SetPixel(offset_x + (10 * block_size), y, 50, 50, 50);  // right
        }

        for (int j = 4; j < 24; j++) {
            for (int i = 0; i < 10; i++) {
                if (grid.matrix[j][i] == 1 || grid.matrix[j][i] > 1) {
                    const RGB &color = (grid.matrix[j][i] == 1) ?
                                       colorList[grid.piece.n] :
                                       colorList[grid.matrix[j][i] - 2];

                    // Draw scaled block
                    for (int x = 0; x < block_size; ++x) {
                        for (int y = 0; y < block_size; ++y) {
                            int pixel_x = offset_x + (i * block_size) + x;
                            int pixel_y = offset_y + ((j - 4) * block_size) + y;

                            if (pixel_x >= 0 && pixel_x < matrix->width() &&
                                pixel_y >= 0 && pixel_y < matrix->height()) {
                                offscreen_canvas->SetPixel(pixel_x, pixel_y, color.r, color.g, color.b);
                            }
                        }
                    }
                }
            }
        }

        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);

        return true;
    }

    std::string TetrisScene::get_name() const {
        return "tetris";
    }

    void TetrisScene::after_render_stop(rgb_matrix::RGBMatrix *matrix) {
        if (gameOver) {
            // Reset game state
            grid = Grid();  // Create new grid
            gameOver = false;
            fixed = false;
            rotated = false;

            // Reset move planning
            bestMove = brain.getBestMove(grid);
            bestRotation = (int) bestMove.back() - 48;
            bestMove.pop_back();

            // Reset fall timing
            last_fall_time = std::chrono::steady_clock::now();
        }
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> TetrisSceneWrapper::create() {
        return std::make_unique<TetrisScene>();
    }
}
