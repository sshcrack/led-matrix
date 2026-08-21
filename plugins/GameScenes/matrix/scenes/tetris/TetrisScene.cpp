#include "TetrisScene.h"
#include <string>
#include "utils/piece.hpp"
#include "utils/grid.hpp"
#include "utils/neuralNetwork.hpp"
#include "spdlog/spdlog.h"
#include <chrono>
#include <array>
#include <algorithm>
#include <cmath>

using namespace std;

namespace {
const std::array<uint8_t, 5> &glyph3x5(char c) {
    static const std::array<uint8_t, 5> blank{0,0,0,0,0};
    static const std::array<uint8_t, 5> zero {7,5,5,5,7};
    static const std::array<uint8_t, 5> one  {2,6,2,2,7};
    static const std::array<uint8_t, 5> two  {7,1,7,4,7};
    static const std::array<uint8_t, 5> three{7,1,7,1,7};
    static const std::array<uint8_t, 5> four {5,5,7,1,1};
    static const std::array<uint8_t, 5> five {7,4,7,1,7};
    static const std::array<uint8_t, 5> six  {7,4,7,5,7};
    static const std::array<uint8_t, 5> seven{7,1,1,1,1};
    static const std::array<uint8_t, 5> eight{7,5,7,5,7};
    static const std::array<uint8_t, 5> nine {7,5,7,1,7};
    static const std::array<uint8_t, 5> S{7,4,7,1,7};
    static const std::array<uint8_t, 5> C{7,4,4,4,7};
    static const std::array<uint8_t, 5> O{7,5,5,5,7};
    static const std::array<uint8_t, 5> R{6,5,6,5,5};
    static const std::array<uint8_t, 5> E{7,4,6,4,7};
    switch(c) {
        case '0': return zero; case '1': return one; case '2': return two; case '3': return three; case '4': return four;
        case '5': return five; case '6': return six; case '7': return seven; case '8': return eight; case '9': return nine;
        case 'S': return S; case 'C': return C; case 'O': return O; case 'R': return R; case 'E': return E;
        default: return blank;
    }
}

void draw3x5(rgb_matrix::FrameCanvas *canvas, int x, int y, const std::string &text,
             uint8_t r, uint8_t g, uint8_t b, int scale = 1) {
    int cursor = x;
    for (char c : text) {
        const auto &rows = glyph3x5(c);
        for (int gy = 0; gy < 5; ++gy) for (int gx = 0; gx < 3; ++gx) {
            if ((rows[gy] & (1 << (2 - gx))) == 0) continue;
            for (int sy = 0; sy < scale; ++sy) for (int sx = 0; sx < scale; ++sx)
                canvas->SetPixel(cursor + gx * scale + sx, y + gy * scale + sy, r, g, b);
        }
        cursor += 4 * scale;
    }
}
}

namespace Scenes {
    TetrisScene::TetrisScene() :
            Scene() {
        bestParams = NeuralNetwork(BEST_PARAMS);
        brain = Brain(bestParams);
        last_update_time = std::chrono::steady_clock::now();
    }

    void TetrisScene::initialize(int width, int height) {
        Scene::initialize(width, height);

        // Calculate scaling and offsets
        block_size = std::min(matrix_width / 10, matrix_height / 20);
        offset_x = (matrix_width - (10 * block_size)) / 2;
        offset_y = (matrix_height - (20 * block_size)) / 2;

        last_update_time = std::chrono::steady_clock::now();
        fall_accumulator_ms = 0.0f;
        move_accumulator = 0.0f;

        bestMove = brain.getBestMove(grid);
        bestRotation = static_cast<int>(bestMove.back()) - 48;
        bestMove.pop_back();
    }

    bool TetrisScene::render(rgb_matrix::FrameCanvas *canvas) {
        if (gameOver) return false;

        const auto current_time = std::chrono::steady_clock::now();
        const float delta_seconds = std::min(0.25f, std::chrono::duration<float>(current_time - last_update_time).count());
        last_update_time = current_time;
        fall_accumulator_ms += delta_seconds * 1000.0f;
        move_accumulator += delta_seconds;

        if (!rotated) {
            for (int i = 0; i < bestRotation; i++) grid.rotatePiece();
            rotated = true;
        }

        if (!bestMove.empty() && !grid.isAnimating && move_accumulator >= move_interval) {
            move_accumulator = std::fmod(move_accumulator, move_interval);
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

        const float fall_step_ms = static_cast<float>(std::max(1, fall_speed_ms->get()));
        int gravity_steps = 0;
        while (fall_accumulator_ms >= fall_step_ms && !grid.isAnimating && gravity_steps < 4) {
            grid.gravity(1);
            fall_accumulator_ms -= fall_step_ms;
            ++gravity_steps;
            if (grid.piece.fixed) break;
        }
        if (gravity_steps == 4) fall_accumulator_ms = 0.0f;

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

        canvas->Clear();

        // Draw game outline
        for (int x = offset_x - 1; x <= offset_x + (10 * block_size); x++) {
            canvas->SetPixel(x, offset_y - 1, 50, 50, 50);  // top
            canvas->SetPixel(x, offset_y + (20 * block_size), 50, 50, 50);  // bottom
        }
        for (int y = offset_y - 1; y <= offset_y + (20 * block_size); y++) {
            canvas->SetPixel(offset_x - 1, y, 50, 50, 50);  // left
            canvas->SetPixel(offset_x + (10 * block_size), y, 50, 50, 50);  // right
        }


        // Score panel. On the 128x128 matrix the board leaves enough room for
        // a permanent, readable HUD; on narrow matrices we use a compact overlay.
        const int board_right = offset_x + 10 * block_size;
        const int right_space = matrix_width - board_right - 2;
        const std::string score_text = std::to_string(grid.score);
        if (right_space >= 20) {
            const int panel_x = board_right + std::max(3, (right_space - 19) / 2);
            const int panel_y = std::max(2, offset_y + 4);
            draw3x5(canvas, panel_x, panel_y, "SCORE", 105, 105, 105);
            draw3x5(canvas, panel_x, panel_y + 8, score_text, 255, 255, 255,
                    score_text.size() <= 2 && right_space >= 28 ? 2 : 1);
        } else {
            const int text_width = static_cast<int>(score_text.size()) * 4 - 1;
            draw3x5(canvas, std::max(offset_x + 1, board_right - text_width - 1), offset_y + 1,
                    score_text, 255, 255, 255);
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

                            if (pixel_x >= 0 && pixel_x < matrix_width &&
                                pixel_y >= 0 && pixel_y < matrix_height) {
                                canvas->SetPixel(pixel_x, pixel_y, color.r, color.g, color.b);
                            }
                        }
                    }
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string TetrisScene::get_name() const {
        return "tetris";
    }

    void TetrisScene::after_render_stop() {
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
            last_update_time = std::chrono::steady_clock::now();
            fall_accumulator_ms = 0.0f;
            move_accumulator = 0.0f;
        }
    }

    std::unique_ptr<Scenes::Scene> TetrisSceneWrapper::create() {
        return std::make_unique<TetrisScene>();
    }
}
