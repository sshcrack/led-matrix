#include "SnakeGameScene.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <queue>

using namespace Scenes;

SnakeGameScene::SnakeGameScene() 
    : rng(std::random_device{}())
{
    set_target_fps(15); // Snake games work well at 15 FPS
}

std::unique_ptr<Scene, void (*)(Scene *)> SnakeGameSceneWrapper::create() {
    return {new SnakeGameScene(), [](Scene *scene) {
        delete scene;
    }};
}

void SnakeGameScene::initialize(rgb_matrix::RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *canvas) {
    Scene::initialize(matrix, canvas);
    initializeGame();
}

void SnakeGameScene::initializeGame() {
    // Reset game state
    snake.clear();
    game_over = false;
    game_won = false;
    score = 0;
    level = 1;
    current_direction = Direction::RIGHT;
    next_direction = Direction::RIGHT;
    death_animation_frame = 0;
    win_animation_frame = 0;
    food_pulse_phase = 0;
    game_over_flash_timer = 0;
    food_eaten = false;
    
    // Initialize snake in the center
    int start_x = matrix_width / 2;
    int start_y = matrix_height / 2;
    
    snake.push_back(Position(start_x, start_y));
    snake.push_back(Position(start_x - 1, start_y));
    snake.push_back(Position(start_x - 2, start_y));
    
    // Initialize distance map
    distance_map.assign(matrix_width, std::vector<int>(matrix_height, -1));
    
    generateFood();
    calculateLevel();
}

bool SnakeGameScene::render(rgb_matrix::RGBMatrixBase *matrix) {
    auto frame = frameTimer.tick();
    
    // Clear canvas
    offscreen_canvas->Clear();
    
    if (game_over) {
        renderGameOver();
    } else if (game_won) {
        renderWin();
    } else {
        // Update game at target framerate
        if (frame.frame % static_cast<int>(get_target_fps() * game_speed) == 0) {
            updateGame();
        }
        renderGame();
    }
    
    return true;
}

void SnakeGameScene::updateGame() {
    // Update AI if enabled
    if (ai_enabled->get()) {
        updateAI();
    }
    
    // Update direction
    current_direction = next_direction;
    
    // Move snake
    moveSnake();
    
    // Update animations
    food_pulse_phase += 0.2f;
    if (food_pulse_phase > 2 * M_PI) food_pulse_phase = 0;
    
    // Check win condition
    if (score >= target_score->get()) {
        game_won = true;
        win_animation_frame = 0;
    }
}

void SnakeGameScene::moveSnake() {
    Position head = snake.front();
    Position new_head = getNextPosition(head, current_direction);
    
    // Handle wrapping if enabled
    if (enable_wrap->get()) {
        if (new_head.x < 0) new_head.x = matrix_width - 1;
        if (new_head.x >= matrix_width) new_head.x = 0;
        if (new_head.y < 0) new_head.y = matrix_height - 1;
        if (new_head.y >= matrix_height) new_head.y = 0;
    }
    
    // Check collision
    if (checkCollision(new_head)) {
        game_over = true;
        death_animation_frame = 0;
        return;
    }
    
    // Move snake
    snake.push_front(new_head);
    
    // Check if food was eaten
    if (new_head == food) {
        score++;
        food_eaten = true;
        generateFood();
        calculateLevel();
    } else {
        snake.pop_back();
    }
}

bool SnakeGameScene::checkCollision(const Position& pos) const {
    // Wall collision (if wrapping is disabled)
    if (!enable_wrap->get()) {
        if (pos.x < 0 || pos.x >= matrix_width || pos.y < 0 || pos.y >= matrix_height) {
            return true;
        }
    }
    
    // Self collision
    for (const auto& segment : snake) {
        if (pos == segment) {
            return true;
        }
    }
    
    return false;
}

void SnakeGameScene::generateFood() {
    std::uniform_int_distribution<int> x_dist(0, matrix_width - 1);
    std::uniform_int_distribution<int> y_dist(0, matrix_height - 1);
    
    do {
        food.x = x_dist(rng);
        food.y = y_dist(rng);
    } while (std::find(snake.begin(), snake.end(), food) != snake.end());
}

void SnakeGameScene::calculateLevel() {
    level = (score / 5) + 1;
    game_speed = base_speed->get() + (level - 1) * 0.05f;
    game_speed = std::min(game_speed, 0.8f); // Cap maximum speed
}

void SnakeGameScene::updateAI() {
    buildDistanceMap(food);
    Direction best_direction = findPathToFood();
    
    if (best_direction != current_direction) {
        // Only change direction if it's valid and won't cause immediate death
        Position test_pos = getNextPosition(snake.front(), best_direction);
        if (!checkCollision(test_pos) && !willCauseSelfTrap(best_direction)) {
            next_direction = best_direction;
        }
    }
}

Direction SnakeGameScene::findPathToFood() {
    Position head = snake.front();
    
    std::vector<Direction> directions = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
    Direction best_dir = current_direction;
    int best_distance = INT_MAX;
    
    for (Direction dir : directions) {
        if (!isValidMove(dir)) continue;
        
        Position next_pos = getNextPosition(head, dir);
        
        // Skip if this would cause collision
        if (checkCollision(next_pos)) continue;
        
        // Use distance map to find best path
        if (distance_map[next_pos.x][next_pos.y] != -1 && distance_map[next_pos.x][next_pos.y] < best_distance) {
            best_distance = distance_map[next_pos.x][next_pos.y];
            best_dir = dir;
        }
    }
    
    return best_dir;
}

bool SnakeGameScene::isValidMove(Direction dir) const {
    // Can't move in opposite direction
    if ((current_direction == Direction::UP && dir == Direction::DOWN) ||
        (current_direction == Direction::DOWN && dir == Direction::UP) ||
        (current_direction == Direction::LEFT && dir == Direction::RIGHT) ||
        (current_direction == Direction::RIGHT && dir == Direction::LEFT)) {
        return false;
    }
    return true;
}

void SnakeGameScene::buildDistanceMap(const Position& target) {
    // Reset distance map
    for (auto& row : distance_map) {
        std::fill(row.begin(), row.end(), -1);
    }
    
    // BFS to calculate distances
    std::queue<Position> queue;
    queue.push(target);
    distance_map[target.x][target.y] = 0;
    
    while (!queue.empty()) {
        Position current = queue.front();
        queue.pop();
        
        std::vector<Direction> directions = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
        
        for (Direction dir : directions) {
            Position next = getNextPosition(current, dir);
            
            // Handle wrapping
            if (enable_wrap->get()) {
                if (next.x < 0) next.x = matrix_width - 1;
                if (next.x >= matrix_width) next.x = 0;
                if (next.y < 0) next.y = matrix_height - 1;
                if (next.y >= matrix_height) next.y = 0;
            }
            
            // Check bounds and visited
            if (next.x < 0 || next.x >= matrix_width || next.y < 0 || next.y >= matrix_height) continue;
            if (distance_map[next.x][next.y] != -1) continue;
            
            // Check if position is blocked by snake
            bool blocked = false;
            for (const auto& segment : snake) {
                if (next == segment) {
                    blocked = true;
                    break;
                }
            }
            
            if (!blocked) {
                distance_map[next.x][next.y] = distance_map[current.x][current.y] + 1;
                queue.push(next);
            }
        }
    }
}

bool SnakeGameScene::willCauseSelfTrap(Direction dir) const {
    // Simple heuristic: check if the move would surround the head
    Position head = snake.front();
    Position next_pos = getNextPosition(head, dir);
    
    if (checkCollision(next_pos)) return true;
    
    // Count available moves from the new position
    int available_moves = 0;
    std::vector<Direction> test_dirs = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
    
    for (Direction test_dir : test_dirs) {
        Position test_pos = getNextPosition(next_pos, test_dir);
        if (!checkCollision(test_pos)) {
            available_moves++;
        }
    }
    
    return available_moves < 2; // Trap if less than 2 escape routes
}

Position SnakeGameScene::getNextPosition(const Position& pos, Direction dir) const {
    switch (dir) {
        case Direction::UP: return Position(pos.x, pos.y - 1);
        case Direction::DOWN: return Position(pos.x, pos.y + 1);
        case Direction::LEFT: return Position(pos.x - 1, pos.y);
        case Direction::RIGHT: return Position(pos.x + 1, pos.y);
    }
    return pos;
}

void SnakeGameScene::renderGame() {
    renderSnake();
    renderFood();
    if (show_score->get()) {
        renderScore();
    }
}

void SnakeGameScene::renderSnake() {
    for (size_t i = 0; i < snake.size(); i++) {
        const Position& segment = snake[i];
        RGB color = getSnakeColor(i);
        
        if (segment.x >= 0 && segment.x < matrix_width && segment.y >= 0 && segment.y < matrix_height) {
            offscreen_canvas->SetPixel(segment.x, segment.y, color.r, color.g, color.b);
        }
    }
}

void SnakeGameScene::renderFood() {
    RGB color = getFoodColor();
    
    if (food.x >= 0 && food.x < matrix_width && food.y >= 0 && food.y < matrix_height) {
        offscreen_canvas->SetPixel(food.x, food.y, color.r, color.g, color.b);
    }
}

void SnakeGameScene::renderScore() {
    // Simple score display in corner
    int display_score = std::min(score, 99); // Limit to 2 digits
    
    // Draw score as small pixels
    for (int i = 0; i < display_score && i < 10; i++) {
        int x = i % 5;
        int y = i / 5;
        offscreen_canvas->SetPixel(matrix_width - 5 + x, y, 100, 100, 255);
    }
}

void SnakeGameScene::renderGameOver() {
    death_animation_frame++;
    
    // Flash effect
    if (death_animation_frame < 60) {
        int flash_intensity = (death_animation_frame % 10 < 5) ? 255 : 100;
        
        // Fill screen with red flash
        for (int y = 0; y < matrix_height; y++) {
            for (int x = 0; x < matrix_width; x++) {
                offscreen_canvas->SetPixel(x, y, flash_intensity, 0, 0);
            }
        }
        
        // Still show snake in different color
        for (const auto& segment : snake) {
            if (segment.x >= 0 && segment.x < matrix_width && segment.y >= 0 && segment.y < matrix_height) {
                offscreen_canvas->SetPixel(segment.x, segment.y, 255, 255, 255);
            }
        }
    } else {
        // Restart game after animation
        initializeGame();
    }
}

void SnakeGameScene::renderWin() {
    win_animation_frame++;
    
    // Rainbow celebration effect
    float hue_offset = win_animation_frame * 0.1f;
    
    for (int y = 0; y < matrix_height; y++) {
        for (int x = 0; x < matrix_width; x++) {
            float hue = fmod(hue_offset + (x + y) * 20.0f, 360.0f);
            
            // Simple HSV to RGB conversion
            float c = 0.8f;
            float x_val = c * (1 - abs(fmod(hue / 60.0f, 2) - 1));
            
            uint8_t r, g, b;
            if (hue < 60) { r = c * 255; g = x_val * 255; b = 0; }
            else if (hue < 120) { r = x_val * 255; g = c * 255; b = 0; }
            else if (hue < 180) { r = 0; g = c * 255; b = x_val * 255; }
            else if (hue < 240) { r = 0; g = x_val * 255; b = c * 255; }
            else if (hue < 300) { r = x_val * 255; g = 0; b = c * 255; }
            else { r = c * 255; g = 0; b = x_val * 255; }
            
            offscreen_canvas->SetPixel(x, y, r, g, b);
        }
    }
    
    // Restart after celebration
    if (win_animation_frame > 120) {
        initializeGame();
    }
}

RGB SnakeGameScene::getSnakeColor(int segment_index) const {
    if (rainbow_snake->get()) {
        // Rainbow snake - each segment has different color
        float hue = fmod(segment_index * 30.0f + frameTimer.get_total_time() * 50.0f, 360.0f);
        
        // Simple HSV to RGB conversion
        float c = 0.9f;
        float x = c * (1 - abs(fmod(hue / 60.0f, 2) - 1));
        
        if (hue < 60) return {static_cast<uint8_t>(c * 255), static_cast<uint8_t>(x * 255), 0};
        else if (hue < 120) return {static_cast<uint8_t>(x * 255), static_cast<uint8_t>(c * 255), 0};
        else if (hue < 180) return {0, static_cast<uint8_t>(c * 255), static_cast<uint8_t>(x * 255)};
        else if (hue < 240) return {0, static_cast<uint8_t>(x * 255), static_cast<uint8_t>(c * 255)};
        else if (hue < 300) return {static_cast<uint8_t>(x * 255), 0, static_cast<uint8_t>(c * 255)};
        else return {static_cast<uint8_t>(c * 255), 0, static_cast<uint8_t>(x * 255)};
    } else {
        // Traditional green snake with gradient
        uint8_t intensity = 255 - (segment_index * 20);
        intensity = std::max(intensity, static_cast<uint8_t>(100));
        
        if (segment_index == 0) {
            return {intensity, 255, intensity}; // Head is brighter
        } else {
            return {0, intensity, 0}; // Body is green
        }
    }
}

RGB SnakeGameScene::getFoodColor() const {
    if (animated_food->get()) {
        // Pulsating food
        float pulse = (sin(food_pulse_phase) + 1.0f) / 2.0f;
        uint8_t intensity = static_cast<uint8_t>(150 + pulse * 105);
        return {intensity, 50, 50};
    } else {
        return {255, 100, 100}; // Static red food
    }
}

std::string SnakeGameScene::get_name() const {
    return "snake_game";
}

void SnakeGameScene::register_properties() {
    add_property(ai_enabled);
    add_property(base_speed);
    add_property(rainbow_snake);
    add_property(enable_wrap);
    add_property(target_score);
    add_property(show_score);
    add_property(animated_food);
}