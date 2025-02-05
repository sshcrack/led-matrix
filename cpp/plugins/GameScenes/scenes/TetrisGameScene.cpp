#include "TetrisGameScene.h"
#include <algorithm>

using namespace Scenes;

TetrisGameScene::TetrisGameScene(const nlohmann::json &config)
        : Scene(config),
          rng(std::random_device{}()),
          drop_time(config.value("drop_time", 0.15f)) {  // Increased speed - lower drop time
    initializeTetrominos();
}

void TetrisGameScene::initializeTetrominos() {
    // I piece
    tetrominos.push_back({{{0, 0, 0, 0},
                           {1, 1, 1, 1},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
    // O piece
    tetrominos.push_back({{{0, 1, 1, 0},
                           {0, 1, 1, 0},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
    // T piece
    tetrominos.push_back({{{0, 1, 0, 0},
                           {1, 1, 1, 0},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
    // L piece
    tetrominos.push_back({{{1, 0, 0, 0},
                           {1, 1, 1, 0},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
    // J piece
    tetrominos.push_back({{{0, 0, 1, 0},
                           {1, 1, 1, 0},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
    // S piece
    tetrominos.push_back({{{0, 1, 1, 0},
                           {1, 1, 0, 0},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
    // Z piece
    tetrominos.push_back({{{1, 1, 0, 0},
                           {0, 1, 1, 0},
                           {0, 0, 0, 0},
                           {0, 0, 0, 0}}});
}

void TetrisGameScene::initialize(rgb_matrix::RGBMatrix *matrix) {
    Scene::initialize(matrix);
    last_update = std::chrono::steady_clock::now();
    accumulated_time = 0.0f;
    restart_timer = 0.0f;
    game_over = false;
    clearing_lines.clear();
    clear_animation_time = 0.0f;

    // Calculate scale while preserving aspect ratio
    float scale_x = matrix->width() / float(BOARD_WIDTH);
    float scale_y = matrix->height() / float(BOARD_HEIGHT);
    scale = std::min(int(scale_x), int(scale_y));

    // Calculate offsets to center the board
    offset_x = (matrix->width() - (BOARD_WIDTH * scale)) / 2;
    offset_y = (matrix->height() - (BOARD_HEIGHT * scale)) / 2;

    // Clear board
    for (auto &row: board) {
        row.fill(0);
    }

    spawnNewPiece();
}

void TetrisGameScene::spawnNewPiece() {
    std::uniform_int_distribution<> tetromino_dist(0, tetrominos.size() - 1);
    std::uniform_int_distribution<> color_dist(1, BLOCK_COLORS);

    current_piece = tetrominos[tetromino_dist(rng)];
    current_x = BOARD_WIDTH / 2 - 2;
    current_y = BOARD_HEIGHT - 4;
    current_color = color_dist(rng);
    
    // Check for collisions at spawn position
    if (!canMove(current_x, current_y, current_piece)) {
        game_over = true;  // If we can't spawn a piece, game is over
    }
}

void TetrisGameScene::getColorComponents(int color, uint8_t &r, uint8_t &g, uint8_t &b) {
    switch (color) {
        case 1:
            r = 255;
            g = 0;
            b = 0;
            break;    // Red
        case 2:
            r = 0;
            g = 255;
            b = 0;
            break;    // Green
        case 3:
            r = 0;
            g = 0;
            b = 255;
            break;    // Blue
        case 4:
            r = 255;
            g = 255;
            b = 0;
            break;  // Yellow
        case 5:
            r = 0;
            g = 255;
            b = 255;
            break;  // Cyan
        case 6:
            r = 255;
            g = 0;
            b = 255;
            break;  // Magenta
        case 7:
            r = 255;
            g = 128;
            b = 0;
            break;  // Orange
        default:
            r = 0;
            g = 0;
            b = 0;
            break;     // Black
    }
}

bool TetrisGameScene::canMove(int new_x, int new_y, const std::array<std::array<bool, 4>, 4> &piece) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (piece[y][x]) {
                int board_x = new_x + x;
                int board_y = new_y + y;
                if (board_x < 0 || board_x >= BOARD_WIDTH ||
                    board_y < 0 || board_y >= BOARD_HEIGHT) {
                    return false;
                }
                // Check for collision with existing pieces
                if (board[board_y][board_x] != 0) {
                    // Ignore collision with current piece's position
                    bool is_current_piece = false;
                    if (current_x <= board_x && board_x < current_x + 4 &&
                        current_y <= board_y && board_y < current_y + 4) {
                        is_current_piece = current_piece[board_y - current_y][board_x - current_x];
                    }
                    if (!is_current_piece) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void TetrisGameScene::mergePiece() {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (current_piece[y][x]) {
                board[current_y + y][current_x + x] = current_color;
            }
        }
    }
}

void TetrisGameScene::clearLines() {
    clearing_lines.clear();
    
    // Find lines to clear
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        bool line_full = true;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] == 0) {
                line_full = false;
                break;
            }
        }
        if (line_full) {
            clearing_lines.push_back(y);
        }
    }
}

void TetrisGameScene::applyLineClears() {
    // Sort lines from top to bottom
    std::sort(clearing_lines.begin(), clearing_lines.end(), std::greater<int>());
    
    // Remove lines and shift down
    for (int line : clearing_lines) {
        // Shift all rows above the cleared line down
        for (int y = line; y < BOARD_HEIGHT - 1; y++) {
            for (int x = 0; x < BOARD_WIDTH; x++) {
                board[y][x] = board[y + 1][x];
            }
        }
        // Clear the top row
        for (int x = 0; x < BOARD_WIDTH; x++) {
            board[BOARD_HEIGHT - 1][x] = 0;
        }
    }
}

std::array<std::array<bool, 4>, 4> TetrisGameScene::rotatePiece(const std::array<std::array<bool, 4>, 4> &piece) {
    std::array<std::array<bool, 4>, 4> rotated{};
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            rotated[y][x] = piece[3 - x][y];
        }
    }
    return rotated;
}

int TetrisGameScene::evaluateLines(const std::vector<std::vector<int>>& temp_board) {
    int lines = 0;
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        int blocks_in_line = 0;
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (temp_board[y][x] != 0) blocks_in_line++;
        }
        if (blocks_in_line == BOARD_WIDTH) {
            lines++;
        } else if (blocks_in_line >= BOARD_WIDTH - 2) {
            // Reward nearly complete lines
            lines += 0.5;
        }
    }
    return lines * 200;  // Increased reward for line clears
}

int TetrisGameScene::evaluateSmoothness(const std::vector<std::vector<int>>& temp_board) {
    int score = 0;
    std::vector<int> heights(BOARD_WIDTH, 0);
    
    // Calculate heights
    for (int x = 0; x < BOARD_WIDTH; x++) {
        for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
            if (temp_board[y][x] != 0) {
                heights[x] = BOARD_HEIGHT - y;
                break;
            }
        }
    }
    
    // Calculate smoothness and bump height differences
    for (int x = 0; x < BOARD_WIDTH - 1; x++) {
        int diff = std::abs(heights[x] - heights[x + 1]);
        score -= diff * diff; // Quadratic penalty for height differences
        
        // Extra penalty for creating peaks
        if (x > 0 && heights[x] > heights[x-1] && heights[x] > heights[x+1]) {
            score -= 20;
        }
    }
    
    return score;
}

int TetrisGameScene::evaluateWells(const std::vector<std::vector<int>>& temp_board) {
    int wells = 0;
    int deep_well_penalty = 0;
    
    for (int x = 0; x < BOARD_WIDTH; x++) {
        int well_depth = 0;
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            if (temp_board[y][x] == 0) {
                bool is_well = true;
                if (x > 0 && temp_board[y][x-1] == 0) is_well = false;
                if (x < BOARD_WIDTH-1 && temp_board[y][x+1] == 0) is_well = false;
                
                if (is_well) {
                    well_depth++;
                    wells++;
                    
                    // Exponential penalty for deep wells
                    if (well_depth > 2) {
                        deep_well_penalty += well_depth * well_depth;
                    }
                }
            } else {
                well_depth = 0;
            }
        }
    }
    
    return -(wells * 10 + deep_well_penalty * 5);
}

int TetrisGameScene::evaluatePosition(int x, int y, const std::array<std::array<bool, 4>, 4>& piece) {
    int score = 0;
    
    // Convert board to vector for easier manipulation
    std::vector<std::vector<int>> temp_board(BOARD_HEIGHT, std::vector<int>(BOARD_WIDTH));
    for (int ty = 0; ty < BOARD_HEIGHT; ty++) {
        for (int tx = 0; tx < BOARD_WIDTH; tx++) {
            temp_board[ty][tx] = board[ty][tx];
        }
    }
    
    // Add piece to temporary board
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (piece[py][px]) {
                temp_board[y + py][x + px] = current_color;
            }
        }
    }
    
    // Core evaluation aspects with adjusted weights
    score += evaluateLines(temp_board) * 5;      // Heavily prioritize line clears
    score += evaluateSmoothness(temp_board) * 3; // Important for maintaining playable stack
    score += evaluateWells(temp_board) * 2;      // Prevent problematic wells
    
    // Count holes and calculate aggregate height
    int holes = 0;
    int aggregate_height = 0;
    int max_height = 0;
    
    for (int x = 0; x < BOARD_WIDTH; x++) {
        bool found_block = false;
        int height = 0;
        for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
            if (temp_board[y][x] != 0) {
                found_block = true;
                height = BOARD_HEIGHT - y;
                aggregate_height += height;
            } else if (found_block) {
                holes++;
            }
        }
        max_height = std::max(max_height, height);
    }
    
    // Adjusted penalties
    score -= holes * 50;              // Severe penalty for holes
    score -= aggregate_height * 3;    // Moderate penalty for overall height
    score -= max_height * 15;         // Extra penalty for tall spikes
    
    // Reduced edge bonus
    if (x == 0 || x + 3 >= BOARD_WIDTH - 1) {
        score -= 20; // Now penalize edge placement
    }
    
    return score;
}

int TetrisGameScene::evaluateSideAttachment(int x, int y, const std::array<std::array<bool, 4>, 4> &piece) {
    int attachments = 0;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (piece[py][px]) {
                if (x + px == 0 || x + px == BOARD_WIDTH - 1) attachments++;
            }
        }
    }
    return attachments * 5;  // Reward side attachments
}

TetrisGameScene::AIMove TetrisGameScene::findBestMove() {
    AIMove best_move{current_x, 0, -999999.0};
    auto test_piece = current_piece;

    // Try all rotations
    for (int rotation = 0; rotation < 4; rotation++) {
        // Try all horizontal positions
        for (int x = -2; x < BOARD_WIDTH + 2; x++) {
            // Find landing position
            int y = current_y;
            while (y > 0 && canMove(x, y - 1, test_piece)) {
                y--;
            }

            if (canMove(x, y, test_piece)) {
                auto simulated_board = simulateMove(x, y, test_piece);
                double score = evaluateBoard(simulated_board);

                if (score > best_move.score) {
                    best_move = {x, rotation, score};
                }
            }
        }
        test_piece = rotatePiece(test_piece);
    }

    return best_move;
}

double TetrisGameScene::evaluateBoard(const std::vector<std::vector<int>>& board) {
    double score = 0;
    
    score += calculateCompleteLines(board) * LINE_CLEAR_WEIGHT;
    score += calculateHoles(board) * HOLE_WEIGHT;
    score += calculateAggregateHeight(board) * HEIGHT_WEIGHT;
    score += calculateRoughness(board) * ROUGHNESS_WEIGHT;
    score += calculateOverhangs(board) * OVERHANG_WEIGHT;
    
    return score;
}

std::vector<std::vector<int>> TetrisGameScene::simulateMove(int x, int y, const std::array<std::array<bool, 4>, 4>& piece) {
    std::vector<std::vector<int>> simulated_board(BOARD_HEIGHT, std::vector<int>(BOARD_WIDTH));
    
    // Copy current board
    for (int i = 0; i < BOARD_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; j++) {
            simulated_board[i][j] = board[i][j];
        }
    }

    // Add piece to simulation
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (piece[py][px] && 
                y + py >= 0 && y + py < BOARD_HEIGHT && 
                x + px >= 0 && x + px < BOARD_WIDTH) {
                simulated_board[y + py][x + px] = current_color;
            }
        }
    }

    return simulated_board;
}

int TetrisGameScene::calculateHoles(const std::vector<std::vector<int>>& board) {
    int holes = 0;
    auto heights = getColumnHeights(board);

    for (int x = 0; x < BOARD_WIDTH; x++) {
        for (int y = 0; y < heights[x]; y++) {
            if (board[y][x] == 0) {
                holes++;
            }
        }
    }
    return holes;
}

int TetrisGameScene::calculateAggregateHeight(const std::vector<std::vector<int>>& board) {
    auto heights = getColumnHeights(board);
    return std::accumulate(heights.begin(), heights.end(), 0);
}

int TetrisGameScene::calculateCompleteLines(const std::vector<std::vector<int>>& board) {
    int complete_lines = 0;
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        if (std::all_of(board[y].begin(), board[y].end(), [](int cell) { return cell != 0; })) {
            complete_lines++;
        }
    }
    return complete_lines;
}

int TetrisGameScene::calculateRoughness(const std::vector<std::vector<int>>& board) {
    auto heights = getColumnHeights(board);
    int roughness = 0;
    
    for (size_t i = 0; i < heights.size() - 1; i++) {
        roughness += std::abs(heights[i] - heights[i + 1]);
    }
    return roughness;
}

int TetrisGameScene::calculateOverhangs(const std::vector<std::vector<int>>& board) {
    int overhangs = 0;
    for (int y = 1; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] == 0 && board[y-1][x] != 0) {
                overhangs++;
            }
        }
    }
    return overhangs;
}

std::vector<int> TetrisGameScene::getColumnHeights(const std::vector<std::vector<int>>& board) {
    std::vector<int> heights(BOARD_WIDTH, 0);
    for (int x = 0; x < BOARD_WIDTH; x++) {
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            if (board[y][x] != 0) {
                heights[x] = BOARD_HEIGHT - y;
                break;
            }
        }
    }
    return heights;
}

void TetrisGameScene::makeAIMove() {
    AIMove best_move = findBestMove();
    
    // Apply rotation
    auto target_piece = current_piece;
    for (int i = 0; i < best_move.rotation; i++) {
        target_piece = rotatePiece(target_piece);
    }
    
    // Apply moves if valid
    if (canMove(best_move.x, current_y, target_piece)) {
        current_piece = target_piece;
        while (current_x < best_move.x && canMove(current_x + 1, current_y, current_piece)) current_x++;
        while (current_x > best_move.x && canMove(current_x - 1, current_y, current_piece)) current_x--;
    }
}

void TetrisGameScene::drawBorder(rgb_matrix::FrameCanvas* canvas) {
    // Draw vertical borders
    for (int y = 0; y < BOARD_HEIGHT * scale; y++) {
        for (int thickness = 0; thickness < 1; thickness++) {
            // Left border
            canvas->SetPixel(offset_x - 1 - thickness, offset_y + y, 
                          BORDER_BRIGHTNESS, BORDER_BRIGHTNESS, BORDER_BRIGHTNESS);
            // Right border
            canvas->SetPixel(offset_x + BOARD_WIDTH * scale + thickness, offset_y + y,
                          BORDER_BRIGHTNESS, BORDER_BRIGHTNESS, BORDER_BRIGHTNESS);
        }
    }
    
    // Draw horizontal borders
    for (int x = offset_x - 1; x < offset_x + BOARD_WIDTH * scale + 1; x++) {
        // Top border
        canvas->SetPixel(x, offset_y - 1, 
                      BORDER_BRIGHTNESS, BORDER_BRIGHTNESS, BORDER_BRIGHTNESS);
        // Bottom border
        canvas->SetPixel(x, offset_y + BOARD_HEIGHT * scale,
                      BORDER_BRIGHTNESS, BORDER_BRIGHTNESS, BORDER_BRIGHTNESS);
    }
}

void TetrisGameScene::updateLineClearAnimation(float delta_time) {
    if (clearing_lines.empty()) return;
    
    clear_animation_time += delta_time;
    if (clear_animation_time >= CLEAR_ANIMATION_DURATION) {
        applyLineClears();
        clear_animation_time = 0.0f;
        clearing_lines.clear();
    }
}

bool TetrisGameScene::render(rgb_matrix::RGBMatrix* matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    
    accumulated_time += delta_time;
    
    if (game_over) {
        restart_timer += delta_time;
        if (restart_timer >= restart_delay) {
            initialize(matrix);
            return true;
        }
        
        // Flash effect for game over state
        bool should_draw = (int)(restart_timer * 2) % 2 == 0;
        if (!should_draw) {
            offscreen_canvas->Fill(0, 0, 0);
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
            return true;
        }
    } else {
        updateLineClearAnimation(delta_time);
        
        if (!isAnimating() && accumulated_time >= drop_time) {
            accumulated_time = 0;
            
            makeAIMove();
            
            if (canMove(current_x, current_y - 1, current_piece)) {
                current_y--;
            } else {
                mergePiece();
                clearLines();
                if (!isLineAnimating()) {
                    spawnNewPiece();
                }
            }
        }
    }
    
    // Clear display
    offscreen_canvas->Fill(0, 0, 0);
    
    // Draw board
    for (int y = 0; y < BOARD_HEIGHT; y++) {
        // Skip lines being cleared during animation
        if (std::find(clearing_lines.begin(), clearing_lines.end(), y) != clearing_lines.end()) {
            float animation_progress = clear_animation_time / CLEAR_ANIMATION_DURATION;
            if ((int)(animation_progress * 10) % 2 == 0) continue; // Flash effect
        }
        
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (board[y][x] != 0) {
                uint8_t r, g, b;
                getColorComponents(board[y][x], r, g, b);
                // Draw scaled block
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        offscreen_canvas->SetPixel(offset_x + x * scale + sx,
                                                   offset_y + (BOARD_HEIGHT - 1 - y) * scale + sy,
                                                   r, g, b);
                    }
                }
            }
        }
    }
    
    // Draw current piece if not animating
    if (!game_over && !isLineAnimating()) {
        uint8_t r, g, b;
        getColorComponents(current_color, r, g, b);
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                if (current_piece[y][x]) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            offscreen_canvas->SetPixel(
                                offset_x + (current_x + x) * scale + sx,
                                offset_y + (BOARD_HEIGHT - 1 - (current_y + y)) * scale + sy,
                                r, g, b);
                        }
                    }
                }
            }
        }
    }
    
    drawBorder(offscreen_canvas);
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
}

string TetrisGameScene::get_name() const {
    return "tetris";
}

Scene *TetrisGameSceneWrapper::create_default() {
    auto config = Scene::create_default(1, 30 * 1000);
    config["drop_time"] = 0.15f;  // Set faster default speed
    return new TetrisGameScene(config);
}

Scene *TetrisGameSceneWrapper::from_json(const nlohmann::json &args) {
    return new TetrisGameScene(args);
}
