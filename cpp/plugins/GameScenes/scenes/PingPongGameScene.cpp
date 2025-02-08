//
// Created by hendrik on 2/5/25.
//

#include "PingPongGameScene.h"

using namespace Scenes;

bool PingPongGameScene::render(rgb_matrix::RGBMatrix *matrix) {
    float ball_size_l = this->ball_size.get();
    float paddle_width_l = this->paddle_width.get();
    float paddle_height_l = this->paddle_height.get();
    float paddle_speed_l = this->paddle_speed.get();

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
    for (int y = 0; y < ball_size_l; y++) {
        for (int x = 0; x < ball_size_l; x++) {
            matrix->SetPixel(int(prev_ball_x) + x, int(prev_ball_y) + y, 0, 0, 0);
        }
    }

    // Clear old paddle positions
    for (int y = 0; y < paddle_height_l; y++) {
        for (int x = 0; x < paddle_width_l; x++) {
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
    ball_x += ball_dx * time_step * 60.0f * curr_speed_multiplier;  // Apply speed multiplier
    ball_y += ball_dy * time_step * 60.0f * curr_speed_multiplier;

    // Simple AI for paddles with time-scaled movement
    float target_y = ball_y - paddle_height_l / 2;
    if (ball_dx < 0) {
        left_paddle_y += (target_y - left_paddle_y) * paddle_speed_l * time_step * 60.0f;
    } else {
        right_paddle_y += (target_y - right_paddle_y) * paddle_speed_l * time_step * 60.0f;
    }

    // Ball collision with top/bottom
    if (ball_y <= 0 || ball_y >= matrix_height - ball_size_l) {
        ball_dy = -ball_dy;
    }

    // Keep paddles within bounds
    left_paddle_y = std::max(0.0f, std::min(left_paddle_y, float(matrix_height - paddle_height_l)));
    right_paddle_y = std::max(0.0f, std::min(right_paddle_y, float(matrix_height - paddle_height_l)));

    // Ball collision with paddles
    if (ball_x <= paddle_width_l && ball_y >= left_paddle_y && ball_y <= left_paddle_y + paddle_height_l) {
        ball_dx = -ball_dx;
        ball_x = paddle_width_l;
        curr_speed_multiplier = std::min(curr_speed_multiplier + 0.1f, max_speed_multiplier.get());
    }
    if (ball_x >= matrix_width - paddle_width_l - ball_size_l &&
        ball_y >= right_paddle_y && ball_y <= right_paddle_y + paddle_height_l) {
        ball_dx = -ball_dx;
        ball_x = matrix_width - paddle_width_l - ball_size_l;
        curr_speed_multiplier = std::min(curr_speed_multiplier + 0.1f, max_speed_multiplier.get());
    }

    // Reset ball if it goes out of bounds
    if (ball_x < 0 || ball_x > matrix_width) {
        ball_x = matrix_width / 2.0f;
        ball_y = matrix_height / 2.0f;
        curr_speed_multiplier = 1.0f;  // Reset speed when ball is reset
    }

    // Draw paddles
    for (int y = 0; y < paddle_height_l; y++) {
        for (int x = 0; x < paddle_width_l; x++) {
            matrix->SetPixel(x, int(left_paddle_y) + y, 255, 255, 255);
            matrix->SetPixel(matrix_width - 1 - x, int(right_paddle_y) + y, 255, 255, 255);
        }
    }

    // Draw ball
    for (int y = 0; y < ball_size_l; y++) {
        for (int x = 0; x < ball_size_l; x++) {
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
    ball_dx = ball_speed.get();
    ball_dy = ball_speed.get();

    // Initialize paddles at center
    left_paddle_y = matrix_height / 2.0f - paddle_height.get() / 2.0f;
    right_paddle_y = matrix_height / 2.0f - paddle_height.get() / 2.0f;

    // Initialize previous positions
    prev_ball_x = ball_x;
    prev_ball_y = ball_y;
    prev_left_paddle_y = left_paddle_y;
    prev_right_paddle_y = right_paddle_y;
}

string PingPongGameScene::get_name() const {
    return "ping_pong";
}

PingPongGameScene::PingPongGameScene() : Scene(false) {
}

void PingPongGameScene::register_properties() {
    add_property(&ball_size, &paddle_width, &paddle_height, &ball_speed, &paddle_speed, &target_fps, &speed_multiplier,
                 &max_speed_multiplier);
}

void PingPongGameScene::load_properties(const json &j) {
    Scene::load_properties(j);

    target_frame_time = 1.0f / target_fps.get();
    curr_speed_multiplier = speed_multiplier.get();
}

Scenes::Scene *PingPongGameSceneWrapper::create() {
    return new PingPongGameScene();
}