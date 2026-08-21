#include "PingPongGameScene.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

const std::array<uint8_t, 5> &digitGlyph(int digit) {
    static const std::array<std::array<uint8_t, 5>, 10> glyphs{{
        {{7,5,5,5,7}}, {{2,6,2,2,7}}, {{7,1,7,4,7}}, {{7,1,7,1,7}}, {{5,5,7,1,1}},
        {{7,4,7,1,7}}, {{7,4,7,5,7}}, {{7,1,1,1,1}}, {{7,5,7,5,7}}, {{7,5,7,1,7}}
    }};
    return glyphs[std::clamp(digit, 0, 9)];
}

void drawDigit(rgb_matrix::FrameCanvas *canvas, int x, int y, int digit) {
    const auto &rows = digitGlyph(digit);
    for (int gy = 0; gy < 5; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
            if (rows[gy] & (1 << (2 - gx))) canvas->SetPixel(x + gx, y + gy, 100, 100, 100);
        }
    }
}
}

namespace Scenes {
PingPongGameScene::PingPongGameScene() : Scene() {}

void PingPongGameScene::reset_ball(int direction) {
    ball_x = (matrix_width - ball_size->get()) * 0.5f;
    ball_y = (matrix_height - ball_size->get()) * 0.5f;
    curr_speed_multiplier = std::max(0.1f, speed_multiplier->get());

    std::uniform_real_distribution<float> angle_distribution(-0.48f, 0.48f);
    const float angle = angle_distribution(random_engine);
    const float base_speed = std::max(1.0f, ball_speed->get() * 60.0f);
    ball_dx = static_cast<float>(direction) * base_speed * std::cos(angle);
    ball_dy = base_speed * std::sin(angle);
}

float PingPongGameScene::predicted_intercept(bool left_side) const {
    const float target_x = left_side ? static_cast<float>(paddle_width->get())
                                     : static_cast<float>(matrix_width - paddle_width->get() - ball_size->get());
    if ((left_side && ball_dx >= 0.0f) || (!left_side && ball_dx <= 0.0f)) return matrix_height * 0.5f;

    const float travel_time = (target_x - ball_x) / ball_dx;
    if (travel_time <= 0.0f) return matrix_height * 0.5f;

    const float range = std::max(1.0f, static_cast<float>(matrix_height - ball_size->get()));
    float projected = ball_y + ball_dy * travel_time;
    const float period = 2.0f * range;
    projected = std::fmod(projected, period);
    if (projected < 0.0f) projected += period;
    if (projected > range) projected = period - projected;
    return projected + ball_size->get() * 0.5f;
}

void PingPongGameScene::update_game(float dt) {
    const float ball_size_l = static_cast<float>(std::max(1, ball_size->get()));
    const float paddle_width_l = static_cast<float>(std::max(1, paddle_width->get()));
    const float paddle_height_l = static_cast<float>(std::clamp(paddle_height->get(), 2, matrix_height));
    const float previous_x = ball_x;

    ball_x += ball_dx * dt * curr_speed_multiplier;
    ball_y += ball_dy * dt * curr_speed_multiplier;

    if (ball_y < 0.0f) {
        ball_y = -ball_y;
        ball_dy = std::abs(ball_dy);
    } else if (ball_y + ball_size_l > matrix_height) {
        ball_y = 2.0f * (matrix_height - ball_size_l) - ball_y;
        ball_dy = -std::abs(ball_dy);
    }

    left_target_y = predicted_intercept(true) - paddle_height_l * 0.5f;
    right_target_y = predicted_intercept(false) - paddle_height_l * 0.5f;

    const float max_paddle_velocity = std::max(8.0f, paddle_speed->get() * 600.0f);
    auto approach = [dt, max_paddle_velocity](float current, float target) {
        const float delta = target - current;
        return current + std::clamp(delta, -max_paddle_velocity * dt, max_paddle_velocity * dt);
    };
    left_paddle_y = approach(left_paddle_y, left_target_y);
    right_paddle_y = approach(right_paddle_y, right_target_y);
    left_paddle_y = std::clamp(left_paddle_y, 0.0f, matrix_height - paddle_height_l);
    right_paddle_y = std::clamp(right_paddle_y, 0.0f, matrix_height - paddle_height_l);

    auto paddle_hit = [&](float paddle_y) {
        return ball_y + ball_size_l >= paddle_y && ball_y <= paddle_y + paddle_height_l;
    };

    const float left_face = paddle_width_l;
    if (ball_dx < 0.0f && previous_x >= left_face && ball_x <= left_face && paddle_hit(left_paddle_y)) {
        ball_x = left_face;
        const float ball_center = ball_y + ball_size_l * 0.5f;
        const float paddle_center = left_paddle_y + paddle_height_l * 0.5f;
        const float normalized = std::clamp((ball_center - paddle_center) / (paddle_height_l * 0.5f), -1.0f, 1.0f);
        const float speed = std::hypot(ball_dx, ball_dy);
        const float angle = normalized * 0.95f;
        ball_dx = std::abs(speed * std::cos(angle));
        ball_dy = speed * std::sin(angle);
        curr_speed_multiplier = std::min(curr_speed_multiplier + 0.08f, max_speed_multiplier->get());
    }

    const float right_face = matrix_width - paddle_width_l - ball_size_l;
    if (ball_dx > 0.0f && previous_x <= right_face && ball_x >= right_face && paddle_hit(right_paddle_y)) {
        ball_x = right_face;
        const float ball_center = ball_y + ball_size_l * 0.5f;
        const float paddle_center = right_paddle_y + paddle_height_l * 0.5f;
        const float normalized = std::clamp((ball_center - paddle_center) / (paddle_height_l * 0.5f), -1.0f, 1.0f);
        const float speed = std::hypot(ball_dx, ball_dy);
        const float angle = normalized * 0.95f;
        ball_dx = -std::abs(speed * std::cos(angle));
        ball_dy = speed * std::sin(angle);
        curr_speed_multiplier = std::min(curr_speed_multiplier + 0.08f, max_speed_multiplier->get());
    }

    if (ball_x + ball_size_l < 0.0f) {
        ++right_score;
        reset_ball(1);
    } else if (ball_x > matrix_width) {
        ++left_score;
        reset_ball(-1);
    }
}

void PingPongGameScene::draw_score(rgb_matrix::FrameCanvas *canvas) const {
    if (matrix_width < 18 || matrix_height < 8) return;
    const int center = matrix_width / 2;
    drawDigit(canvas, center - 6, 2, left_score % 10);
    drawDigit(canvas, center + 3, 2, right_score % 10);
}

bool PingPongGameScene::render(rgb_matrix::FrameCanvas *canvas) {
    const auto current_time = std::chrono::steady_clock::now();
    float delta_time = std::chrono::duration<float>(current_time - last_update).count();
    last_update = current_time;
    delta_time = std::min(delta_time, 0.25f);
    accumulated_time += delta_time;

    const float step = std::max(1.0f / 240.0f, target_frame_time);
    int updates = 0;
    while (accumulated_time >= step && updates < 8) {
        update_game(step);
        accumulated_time -= step;
        ++updates;
    }
    if (updates == 8) accumulated_time = 0.0f;

    canvas->Clear();

    for (int y = 0; y < matrix_height; y += 4) {
        canvas->SetPixel(matrix_width / 2, y, 45, 45, 45);
        if (y + 1 < matrix_height) canvas->SetPixel(matrix_width / 2, y + 1, 45, 45, 45);
    }

    const int paddle_width_l = std::max(1, paddle_width->get());
    const int paddle_height_l = std::clamp(paddle_height->get(), 2, matrix_height);
    for (int y = 0; y < paddle_height_l; ++y) {
        for (int x = 0; x < paddle_width_l; ++x) {
            canvas->SetPixel(x, static_cast<int>(left_paddle_y) + y, 255, 255, 255);
            canvas->SetPixel(matrix_width - 1 - x, static_cast<int>(right_paddle_y) + y, 255, 255, 255);
        }
    }

    const int ball_size_l = std::max(1, ball_size->get());
    for (int y = 0; y < ball_size_l; ++y) {
        for (int x = 0; x < ball_size_l; ++x) {
            const int px = static_cast<int>(std::round(ball_x)) + x;
            const int py = static_cast<int>(std::round(ball_y)) + y;
            if (px >= 0 && px < matrix_width && py >= 0 && py < matrix_height)
                canvas->SetPixel(px, py, 255, 255, 255);
        }
    }

    draw_score(canvas);
    wait_until_next_frame();
    return true;
}

void PingPongGameScene::initialize(int width, int height) {
    Scene::initialize(width, height);
    target_frame_time = 1.0f / std::max(1.0f, target_fps->get());
    last_update = std::chrono::steady_clock::now();
    accumulated_time = 0.0f;
    left_score = 0;
    right_score = 0;
    left_paddle_y = matrix_height * 0.5f - paddle_height->get() * 0.5f;
    right_paddle_y = left_paddle_y;
    left_target_y = left_paddle_y;
    right_target_y = right_paddle_y;
    std::uniform_int_distribution<int> direction(0, 1);
    reset_ball(direction(random_engine) == 0 ? -1 : 1);
}

std::string PingPongGameScene::get_name() const { return "ping_pong"; }

void PingPongGameScene::register_properties() {
    add_property(ball_size);
    add_property(paddle_width);
    add_property(paddle_height);
    add_property(ball_speed);
    add_property(paddle_speed);
    add_property(target_fps);
    add_property(speed_multiplier);
    add_property(max_speed_multiplier);
}

void PingPongGameScene::load_properties(const json &j) {
    Scene::load_properties(j);
    target_frame_time = 1.0f / std::max(1.0f, target_fps->get());
    curr_speed_multiplier = std::max(0.1f, speed_multiplier->get());
}

std::unique_ptr<Scenes::Scene> PingPongGameSceneWrapper::create() {
    return std::make_unique<PingPongGameScene>();
}
} // namespace Scenes
