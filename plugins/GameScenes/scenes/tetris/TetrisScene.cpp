#include "TetrisScene.h"
#include <string>
#include "utils/piece.hpp"
#include "utils/grid.hpp"
#include "utils/neuralNetwork.hpp"
#include "spdlog/spdlog.h"
#include "shared/matrix/execution_mode.h"
#include "shared/matrix/utils/shared.h"
#include <chrono>
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>

using namespace std;

namespace {
constexpr const char *game_scenes_config_key = "GameScenes";
std::atomic<int> process_high_score{0};

int load_persisted_high_score() {
    if (SceneExecution::mode() != SceneExecution::Mode::Matrix || config == nullptr) return 0;
    try {
        const auto plugin_configs = config->get_plugin_configs();
        const auto it = plugin_configs.find(game_scenes_config_key);
        if (it == plugin_configs.end()) return 0;
        const auto state = nlohmann::json::parse(it->second);
        return std::max(0, state.value("tetris_high_score", 0));
    } catch (const std::exception &e) {
        spdlog::warn("Ignoring invalid GameScenes persistent state: {}", e.what());
        return 0;
    }
}

void publish_high_score(int score) {
    int current = process_high_score.load(std::memory_order_relaxed);
    while (score > current &&
           !process_high_score.compare_exchange_weak(current, score,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed)) {
    }
}

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
    static const std::array<uint8_t, 5> H{5,5,7,5,5};
    static const std::array<uint8_t, 5> I{7,2,2,2,7};
    switch(c) {
        case '0': return zero; case '1': return one; case '2': return two; case '3': return three; case '4': return four;
        case '5': return five; case '6': return six; case '7': return seven; case '8': return eight; case '9': return nine;
        case 'S': return S; case 'C': return C; case 'O': return O; case 'R': return R; case 'E': return E;
        case 'H': return H; case 'I': return I;
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
        high_score = std::max(load_persisted_high_score(),
                              process_high_score.load(std::memory_order_relaxed));
        publish_high_score(high_score);
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

    void TetrisScene::update_high_score() {
        const int previous = high_score;
        high_score = std::max(high_score, grid.score);
        if (!SceneExecution::is_preview()) {
            publish_high_score(high_score);
            high_score = std::max(high_score, process_high_score.load(std::memory_order_relaxed));
        }
        if (SceneExecution::mode() == SceneExecution::Mode::Matrix && high_score > previous)
            high_score_dirty = true;
    }

    void TetrisScene::persist_high_score() {
        if (!high_score_dirty || SceneExecution::mode() != SceneExecution::Mode::Matrix || config == nullptr)
            return;

        try {
            auto plugin_configs = config->get_plugin_configs();
            nlohmann::json state = nlohmann::json::object();
            if (const auto it = plugin_configs.find(game_scenes_config_key); it != plugin_configs.end()) {
                try { state = nlohmann::json::parse(it->second); } catch (...) {}
                if (!state.is_object()) state = nlohmann::json::object();
            }
            state["tetris_high_score"] = high_score;
            config->set_plugin_config(game_scenes_config_key, state.dump());
            high_score_dirty = false;
        } catch (const std::exception &e) {
            spdlog::warn("Could not persist Tetris high score: {}", e.what());
        }
    }

    bool TetrisScene::render(rgb_matrix::FrameCanvas *canvas) {
        if (gameOver) {
            hold_current_frame();
            return false;
        }

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

        update_high_score();

        if (grid.gameOver) {
            gameOver = true;
            hold_current_frame();
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


        // Score panel. On wide matrices both the current score and session high
        // high score get a permanent HUD. Narrow matrices keep both values inside the
        // board header; if unusually large values no longer fit, alternate them.
        const int board_right = offset_x + 10 * block_size;
        const int right_space = matrix_width - board_right - 2;
        const std::string score_text = std::to_string(grid.score);
        const std::string high_score_text = std::to_string(high_score);
        if (right_space >= 20) {
            const int panel_x = board_right + std::max(3, (right_space - 19) / 2);
            const int panel_y = std::max(2, offset_y + 4);
            const int score_scale = score_text.size() <= 2 && right_space >= 28 ? 2 : 1;
            const int high_scale = high_score_text.size() <= 2 && right_space >= 28 ? 2 : 1;
            draw3x5(canvas, panel_x, panel_y, "SCORE", 105, 105, 105);
            draw3x5(canvas, panel_x, panel_y + 8, score_text, 255, 255, 255, score_scale);

            const int high_label_y = panel_y + (score_scale == 2 ? 21 : 15);
            draw3x5(canvas, panel_x, high_label_y, "HI", 105, 105, 105);
            draw3x5(canvas, panel_x, high_label_y + 8, high_score_text, 255, 220, 80, high_scale);
        } else {
            const std::string compact_high = "H" + high_score_text;
            const int score_width = static_cast<int>(score_text.size()) * 4 - 1;
            const int high_width = static_cast<int>(compact_high.size()) * 4 - 1;
            const int inner_width = std::max(0, 10 * block_size - 2);
            const int header_y = offset_y + 1;

            if (score_width + high_width + 2 <= inner_width) {
                draw3x5(canvas, offset_x + 1, header_y, compact_high, 255, 220, 80);
                draw3x5(canvas, board_right - score_width - 1, header_y,
                        score_text, 255, 255, 255);
            } else {
                const bool show_high = (frame_context().now_ms / 2000U) % 2U != 0U;
                const std::string &text = show_high ? compact_high : score_text;
                const int text_width = static_cast<int>(text.size()) * 4 - 1;
                const uint8_t g = show_high ? 220 : 255;
                const uint8_t b = show_high ? 80 : 255;
                draw3x5(canvas, std::max(offset_x + 1, board_right - text_width - 1), header_y,
                        text, 255, g, b);
            }
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

    nlohmann::json TetrisScene::snapshot_runtime_state() const {
        return {{"high_score", high_score}};
    }

    void TetrisScene::restore_runtime_state(const nlohmann::json &state) {
        const int restored = std::max(0, state.value("high_score", 0));
        high_score = std::max({high_score, restored, grid.score});
        if (!SceneExecution::is_preview()) {
            publish_high_score(high_score);
            high_score = std::max(high_score, process_high_score.load(std::memory_order_relaxed));
        }
    }

    std::string TetrisScene::get_name() const {
        return "tetris";
    }

    void TetrisScene::after_render_stop() {
        persist_high_score();

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
