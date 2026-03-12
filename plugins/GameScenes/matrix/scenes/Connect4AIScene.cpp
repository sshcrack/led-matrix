#include "Connect4AIScene.h"
#include <algorithm>
#include <cmath>

namespace Scenes {
    Connect4AIScene::Connect4AIScene()
        : Scene(), current_player(PLAYER1), game_state(0), move_count(0),
          animation_frame(0), last_drop_col(-1), rng(std::random_device{}()) {
    }

    void Connect4AIScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        init_game();
    }

    void Connect4AIScene::init_game() {
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                board[r][c] = EMPTY;
            }
        }
        current_player = PLAYER1;
        game_state = 0;
        move_count = 0;
        animation_frame = 0;
        last_drop_col = -1;
    }

    bool Connect4AIScene::drop_piece(int col) {
        if (col < 0 || col >= COLS) return false;

        // Find the lowest empty row in this column
        for (int r = ROWS - 1; r >= 0; r--) {
            if (board[r][col] == EMPTY) {
                board[r][col] = current_player;
                last_drop_col = col;
                move_count++;
                return true;
            }
        }
        return false;
    }

    bool Connect4AIScene::check_win(uint8_t player) {
        // Check horizontal
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS - 3; c++) {
                if (board[r][c] == player && board[r][c + 1] == player &&
                    board[r][c + 2] == player && board[r][c + 3] == player) {
                    return true;
                }
            }
        }

        // Check vertical
        for (int c = 0; c < COLS; c++) {
            for (int r = 0; r < ROWS - 3; r++) {
                if (board[r][c] == player && board[r + 1][c] == player &&
                    board[r + 2][c] == player && board[r + 3][c] == player) {
                    return true;
                }
            }
        }

        // Check diagonal (positive slope)
        for (int r = 3; r < ROWS; r++) {
            for (int c = 0; c < COLS - 3; c++) {
                if (board[r][c] == player && board[r - 1][c + 1] == player &&
                    board[r - 2][c + 2] == player && board[r - 3][c + 3] == player) {
                    return true;
                }
            }
        }

        // Check diagonal (negative slope)
        for (int r = 0; r < ROWS - 3; r++) {
            for (int c = 0; c < COLS - 3; c++) {
                if (board[r][c] == player && board[r + 1][c + 1] == player &&
                    board[r + 2][c + 2] == player && board[r + 3][c + 3] == player) {
                    return true;
                }
            }
        }

        return false;
    }

    int Connect4AIScene::evaluate_board(uint8_t player) {
        uint8_t opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;
        int score = 0;

        // Check for 3-in-a-row patterns
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS - 2; c++) {
                int count = 0, empty = 0;
                for (int i = 0; i < 4; i++) {
                    if (board[r][c + i] == player) count++;
                    else if (board[r][c + i] == EMPTY) empty++;
                }
                if (count > 0 && empty >= COLS - count - 1) score += count * count * 10;
            }
        }

        // Penalize opponent's potential
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS - 2; c++) {
                int count = 0;
                for (int i = 0; i < 3; i++) {
                    if (board[r][c + i] == opponent) count++;
                }
                if (count == 3) score -= 1000;
            }
        }

        // Prefer center columns
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (board[r][c] == player) {
                    score += (3 - std::abs(c - 3)) * 2;
                }
            }
        }

        return score;
    }

    int Connect4AIScene::ai_minimax(int col, int depth, bool is_maximizing, int alpha, int beta) {
        if (depth == 0 || move_count >= ROWS * COLS) {
            return evaluate_board(PLAYER2);
        }

        if (is_maximizing) {
            int max_eval = -10000;
            for (int c = 0; c < COLS; c++) {
                // Try move
                bool moved = drop_piece(c);
                if (!moved) continue;

                int current_eval;
                if (check_win(PLAYER2)) {
                    current_eval = 10000;
                } else {
                    current_eval = ai_minimax(c, depth - 1, false, alpha, beta);
                }

                // Undo move
                for (int r = 0; r < ROWS; r++) {
                    if (board[r][c] == PLAYER2) {
                        board[r][c] = EMPTY;
                        move_count--;
                        break;
                    }
                }

                max_eval = std::max(max_eval, current_eval);
                alpha = std::max(alpha, current_eval);
                if (beta <= alpha) break;
            }
            return max_eval;
        } else {
            int min_eval = 10000;
            for (int c = 0; c < COLS; c++) {
                bool moved = drop_piece(c);
                if (!moved) continue;

                int current_eval;
                if (check_win(PLAYER1)) {
                    current_eval = -10000;
                } else {
                    current_eval = ai_minimax(c, depth - 1, true, alpha, beta);
                }

                // Undo move
                for (int r = 0; r < ROWS; r++) {
                    if (board[r][c] == PLAYER1) {
                        board[r][c] = EMPTY;
                        move_count--;
                        break;
                    }
                }

                min_eval = std::min(min_eval, current_eval);
                beta = std::min(beta, current_eval);
                if (beta <= alpha) break;
            }
            return min_eval;
        }
    }

    int Connect4AIScene::ai_get_best_move() {
        int best_col = 3; // Default to center
        int best_score = -10000;
        int depth = difficulty->get();

        for (int c = 0; c < COLS; c++) {
            bool moved = drop_piece(c);
            if (!moved) continue;

            int score;
            if (check_win(PLAYER2)) {
                score = 10000;
            } else {
                score = ai_minimax(c, depth - 1, false, -10000, 10000);
            }

            // Undo move
            for (int r = 0; r < ROWS; r++) {
                if (board[r][c] == PLAYER2) {
                    board[r][c] = EMPTY;
                    move_count--;
                    break;
                }
            }

            if (score > best_score) {
                best_score = score;
                best_col = c;
            }
        }

        return best_col;
    }

    void Connect4AIScene::render_board(RGBMatrixBase *matrix) {
        // Board is 7 wide, 6 tall = 14x12 on pixel grid (2 pixels per cell)
        int start_x = 55;
        int start_y = 58;
        int cell_size = 2;

        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                int px = start_x + c * cell_size;
                int py = start_y + r * cell_size;

                uint8_t r_val = 20, g_val = 20, b_val = 50; // Board background

                if (board[r][c] == PLAYER1) {
                    r_val = 255; g_val = 100; b_val = 100; // Red
                } else if (board[r][c] == PLAYER2) {
                    r_val = 255; g_val = 255; b_val = 100; // Yellow
                }

                for (int dx = 0; dx < cell_size; dx++) {
                    for (int dy = 0; dy < cell_size; dy++) {
                        if (px + dx < matrix->width() && py + dy < matrix->height()) {
                            offscreen_canvas->SetPixel(px + dx, py + dy, r_val, g_val, b_val);
                        }
                    }
                }
            }
        }
    }

    bool Connect4AIScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

        // Draw board
        render_board(matrix);

        // Draw score or game state
        if (game_state == 0) {
            // Game is playing
            if (current_player == PLAYER2) {
                // AI is thinking/making move
                static int ai_delay = 0;
                if (ai_delay == 0) {
                    int best_col = ai_get_best_move();
                    drop_piece(best_col);

                    if (check_win(PLAYER2)) {
                        game_state = PLAYER2;
                    } else if (move_count >= ROWS * COLS) {
                        game_state = 3; // Draw
                    } else {
                        current_player = PLAYER1;
                    }
                    ai_delay = static_cast<int>(30 / ai_speed->get());
                }
                ai_delay--;
            } else {
                // Player 1 makes random moves (AI vs AI)
                static int p1_delay = 0;
                if (p1_delay == 0) {
                    std::uniform_int_distribution<> dis(0, COLS - 1);
                    int col = dis(rng);
                    if (drop_piece(col)) {
                        if (check_win(PLAYER1)) {
                            game_state = PLAYER1;
                        } else if (move_count >= ROWS * COLS) {
                            game_state = 3; // Draw
                        } else {
                            current_player = PLAYER2;
                        }
                    }
                    p1_delay = 20;
                }
                p1_delay--;
            }
        } else {
            // Game over - show result and restart after delay
            animation_frame++;
            if (animation_frame > 200) {
                init_game();
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string Connect4AIScene::get_name() const {
        return "connect4_ai";
    }

    void Connect4AIScene::register_properties() {
        add_property(ai_speed);
        add_property(difficulty);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> Connect4AISceneWrapper::create() {
        return {new Connect4AIScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<Connect4AIScene *>(scene);
        }};
    }
}
