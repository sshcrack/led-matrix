//
// Created by hendrik on 2/5/25.
//

#include "PingPongGameScene.h"

using namespace Scenes;

bool PingPongGameScene::render(rgb_matrix::RGBMatrix *matrix) {
    auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    
    accumulated_time += delta_time;
    if (accumulated_time < target_frame_time) {
        return true;  // Skip frame if not enough time has passed
    }

    // Update game with consistent time step
    float time_step = target_frame_time;
    accumulated_time -= target_frame_time;

    // Clear previous positions first
    // Clear old ball position
    for (int y = 0; y < ball_size; y++) {
        for (int x = 0; x < ball_size; x++) {
            matrix->SetPixel(int(prev_ball_x) + x, int(prev_ball_y) + y, 0, 0, 0);
        }
    }

    // Clear old paddle positions
    for (int y = 0; y < paddle_height; y++) {
        for (int x = 0; x < paddle_width; x++) {
            matrix->SetPixel(x, int(prev_left_paddle_y) + y, 0, 0, 0);
            matrix->SetPixel(matrix_width - 1 - x, int(prev_right_paddle_y) + y, 0, 0, 0);
        }
    }

    // Store current positions as previous
    prev_ball_x = ball_x;
    prev_ball_y = ball_y;
    prev_left_paddle_y = left_paddle_y;
    prev_right_paddle_y = right_paddle_y;

    // Update ball position with time-scaled movement
    ball_x += ball_dx * time_step * 60.0f * speed_multiplier;  // Apply speed multiplier
    ball_y += ball_dy * time_step * 60.0f * speed_multiplier;

    // Simple AI for paddles with time-scaled movement
    float target_y = ball_y - paddle_height / 2;
    if (ball_dx < 0) {
        left_paddle_y += (target_y - left_paddle_y) * paddle_speed * time_step * 60.0f;
    } else {
        right_paddle_y += (target_y - right_paddle_y) * paddle_speed * time_step * 60.0f;
    }

    // Ball collision with top/bottom
    if (ball_y <= 0 || ball_y >= matrix_height - ball_size) {
        ball_dy = -ball_dy;
    }

    // Keep paddles within bounds
    left_paddle_y = std::max(0.0f, std::min(left_paddle_y, float(matrix_height - paddle_height)));
    right_paddle_y = std::max(0.0f, std::min(right_paddle_y, float(matrix_height - paddle_height)));

    // Ball collision with paddles
    if (ball_x <= paddle_width && ball_y >= left_paddle_y && ball_y <= left_paddle_y + paddle_height) {
        ball_dx = -ball_dx;
        ball_x = paddle_width;
        speed_multiplier = std::min(speed_multiplier + 0.1f, max_speed_multiplier);
    }
    if (ball_x >= matrix_width - paddle_width - ball_size &&
        ball_y >= right_paddle_y && ball_y <= right_paddle_y + paddle_height) {
        ball_dx = -ball_dx;
        ball_x = matrix_width - paddle_width - ball_size;
        speed_multiplier = std::min(speed_multiplier + 0.1f, max_speed_multiplier);
    }

    // Reset ball if it goes out of bounds
    if (ball_x < 0 || ball_x > matrix_width) {
        ball_x = matrix_width / 2.0f;
        ball_y = matrix_height / 2.0f;
        speed_multiplier = 1.0f;  // Reset speed when ball is reset
    }

    // Draw paddles
    for (int y = 0; y < paddle_height; y++) {
        for (int x = 0; x < paddle_width; x++) {
            matrix->SetPixel(x, int(left_paddle_y) + y, 255, 255, 255);
            matrix->SetPixel(matrix_width - 1 - x, int(right_paddle_y) + y, 255, 255, 255);
        }
    }

    // Draw ball
    for (int y = 0; y < ball_size; y++) {
        for (int x = 0; x < ball_size; x++) {
            matrix->SetPixel(int(ball_x) + x, int(ball_y) + y, 255, 255, 255);
        }
    }
    return true;
}

void PingPongGameScene::initialize(rgb_matrix::RGBMatrix *matrix) {
    Scene::initialize(matrix);
    last_update = std::chrono::steady_clock::now();
    accumulated_time = 0.0f;

    // Initialize ball at center
    ball_x = matrix_width / 2.0f;
    ball_y = matrix_height / 2.0f;
    ball_dx = ball_speed;
    ball_dy = ball_speed;

    // Initialize paddles at center
    left_paddle_y = matrix_height / 2.0f - paddle_height / 2.0f;
    right_paddle_y = matrix_height / 2.0f - paddle_height / 2.0f;

    // Initialize previous positions
    prev_ball_x = ball_x;
    prev_ball_y = ball_y;
    prev_left_paddle_y = left_paddle_y;
    prev_right_paddle_y = right_paddle_y;
}

string PingPongGameScene::get_name() const {
    return "ping_pong";
}

PingPongGameScene::PingPongGameScene(const json &config) : Scene(config, false) {
    ball_size = config.value("ball_size", 2);
    paddle_width = config.value("paddle_width", 2);
    paddle_height = config.value("paddle_height", 8);
    ball_speed = config.value("ball_speed", 0.3f);     // Increased from 0.1f
    paddle_speed = config.value("paddle_speed", 0.15f); // Increased from 0.05f
    target_frame_time = 1.0f / config.value("target_fps", 60.0f);
    speed_multiplier = config.value("speed_multiplier", 1.0f);
    max_speed_multiplier = config.value("max_speed_multiplier", 4.0f);
}

Scenes::Scene *PingPongGameSceneWrapper::create_default() {
    return new PingPongGameScene(Scene::create_default(3, 10 * 1000));
}

Scenes::Scene *PingPongGameSceneWrapper::from_json(const json &args) {
    return new PingPongGameScene(args);
}
