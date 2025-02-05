//
// Created by hendrik on 2/5/25.
//

#include "PingPongGameScene.h"

using namespace Scenes;

bool PingPongGameScene::render(rgb_matrix::RGBMatrix *matrix) {
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

    // Clear matrix
    matrix->Clear();

    // Update ball position
    ball_x += ball_dx;
    ball_y += ball_dy;

    // Ball collision with top/bottom
    if (ball_y <= 0 || ball_y >= matrix_height - ball_size) {
        ball_dy = -ball_dy;
    }

    // Simple AI for paddles
    float target_y = ball_y - paddle_height / 2;
    if (ball_dx < 0) {
        left_paddle_y += (target_y - left_paddle_y) * paddle_speed;
    } else {
        right_paddle_y += (target_y - right_paddle_y) * paddle_speed;
    }

    // Keep paddles within bounds
    left_paddle_y = std::max(0.0f, std::min(left_paddle_y, float(matrix_height - paddle_height)));
    right_paddle_y = std::max(0.0f, std::min(right_paddle_y, float(matrix_height - paddle_height)));

    // Ball collision with paddles
    if (ball_x <= paddle_width && ball_y >= left_paddle_y && ball_y <= left_paddle_y + paddle_height) {
        ball_dx = -ball_dx;
        ball_x = paddle_width;
    }
    if (ball_x >= matrix_width - paddle_width - ball_size &&
        ball_y >= right_paddle_y && ball_y <= right_paddle_y + paddle_height) {
        ball_dx = -ball_dx;
        ball_x = matrix_width - paddle_width - ball_size;
    }

    // Reset ball if it goes out of bounds
    if (ball_x < 0 || ball_x > matrix_width) {
        ball_x = matrix_width / 2.0f;
        ball_y = matrix_height / 2.0f;
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
    ball_speed = config.value("ball_speed", 0.5f);
    paddle_speed = config.value("paddle_speed", 0.3f);
}

Scenes::Scene *PingPongGameSceneWrapper::create_default() {
    return new PingPongGameScene(Scene::create_default(3, 10 * 1000));
}

Scenes::Scene *PingPongGameSceneWrapper::from_json(const json &args) {
    return new PingPongGameScene(args);
}
