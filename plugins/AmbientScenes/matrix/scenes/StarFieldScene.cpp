#include "StarFieldScene.h"
#include <algorithm>
#include <cmath>

namespace AmbientScenes {
    void StarFieldScene::Star::respawn(float max_depth, std::mt19937& rng) {
        std::uniform_real_distribution<float> coord(-1.0f, 1.0f);
        std::uniform_real_distribution<float> tint(0.0f, 1.0f);
        do { x = coord(rng); y = coord(rng); } while (x * x + y * y < 0.025f);
        z = max_depth;
        previous_z = z;
        hue = tint(rng);
    }

    void StarFieldScene::Star::update(float amount) {
        previous_z = z;
        z -= amount;
    }

    StarFieldScene::StarFieldScene() : Scene(), gen(rd()), dis(0.0, 1.0) {}

    void StarFieldScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        stars.resize(std::max(1, num_stars->get()));
        const float depth = std::max(0.2f, max_depth->get());
        std::uniform_real_distribution<float> zdist(0.08f, depth);
        for (auto& star : stars) {
            star.respawn(depth, gen);
            star.z = zdist(gen);
            star.previous_z = std::min(depth, star.z + speed->get());
        }
        time = 0.0f;
        last_update = std::chrono::steady_clock::now();
    }

    void StarFieldScene::hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
        h = h - std::floor(h);
        const float c = v * s;
        const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        const float m = v - c;
        float rr = 0, gg = 0, bb = 0;
        const int sector = static_cast<int>(h * 6.0f) % 6;
        if (sector == 0) { rr = c; gg = x; }
        else if (sector == 1) { rr = x; gg = c; }
        else if (sector == 2) { gg = c; bb = x; }
        else if (sector == 3) { gg = x; bb = c; }
        else if (sector == 4) { rr = x; bb = c; }
        else { rr = c; bb = x; }
        r = static_cast<uint8_t>(std::clamp((rr + m) * 255.0f, 0.0f, 255.0f));
        g = static_cast<uint8_t>(std::clamp((gg + m) * 255.0f, 0.0f, 255.0f));
        b = static_cast<uint8_t>(std::clamp((bb + m) * 255.0f, 0.0f, 255.0f));
    }

    void StarFieldScene::draw_line(rgb_matrix::FrameCanvas* canvas, int x0, int y0, int x1, int y1,
                                   uint8_t r, uint8_t g, uint8_t b, int width, int height) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) canvas->SetPixel(x0, y0, r, g, b);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    bool StarFieldScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::min(0.10f, std::chrono::duration<float>(now - last_update).count());
        last_update = now;
        time += dt;
        const float depth = std::max(0.2f, max_depth->get());

        const std::size_t wanted_stars = static_cast<std::size_t>(std::max(1, num_stars->get()));
        if (stars.size() != wanted_stars) {
            const std::size_t old_size = stars.size();
            stars.resize(wanted_stars);
            for (std::size_t i = old_size; i < stars.size(); ++i) stars[i].respawn(depth, gen);
        }
        const float cx = matrix_width * 0.5f + (drifting_center->get() ? std::sin(time * 0.63f) * matrix_width * 0.075f : 0.0f);
        const float cy = matrix_height * 0.5f + (drifting_center->get() ? std::cos(time * 0.47f) * matrix_height * 0.06f : 0.0f);
        const float scale = static_cast<float>(std::min(matrix_width, matrix_height)) * 0.48f;

        for (auto& star : stars) {
            const float movement = std::max(0.001f, speed->get()) * dt * 60.0f;
            star.update(movement);
            if (star.z <= 0.025f) star.respawn(depth, gen);

            const float inv_z = 1.0f / star.z;
            const float inv_prev = 1.0f / std::max(star.z + std::max(0.001f, speed->get()) * streak_length->get() * 6.0f, 0.025f);
            const int x = static_cast<int>(cx + star.x * scale * inv_z);
            const int y = static_cast<int>(cy + star.y * scale * inv_z);
            const int px = static_cast<int>(cx + star.x * scale * inv_prev);
            const int py = static_cast<int>(cy + star.y * scale * inv_prev);

            if (x < -8 || x >= matrix_width + 8 || y < -8 || y >= matrix_height + 8) {
                star.respawn(depth, gen);
                continue;
            }

            float brightness = std::clamp((depth - star.z) / depth, 0.05f, 1.0f);
            brightness = std::sqrt(brightness);
            if (enable_twinkle->get()) brightness *= 0.82f + 0.18f * std::sin(time * 7.0f + star.hue * 31.0f);

            uint8_t r, g, b;
            if (colored_stars->get()) {
                const float hue = 0.55f + (star.hue - 0.5f) * 0.18f;
                hsv_to_rgb(hue, 0.28f, std::clamp(brightness, 0.0f, 1.0f), r, g, b);
            } else {
                r = g = b = static_cast<uint8_t>(255.0f * std::clamp(brightness, 0.0f, 1.0f));
            }

            if (streak_length->get() > 0.01f)
                draw_line(canvas, px, py, x, y, r / 2, g / 2, b / 2, matrix_width, matrix_height);
            if (x >= 0 && x < matrix_width && y >= 0 && y < matrix_height) {
                canvas->SetPixel(x, y, r, g, b);
                if (star.z < depth * 0.22f) {
                    const uint8_t gr = r / 4, gg = g / 4, gb = b / 4;
                    if (x > 0) canvas->SetPixel(x - 1, y, gr, gg, gb);
                    if (x + 1 < matrix_width) canvas->SetPixel(x + 1, y, gr, gg, gb);
                    if (y > 0) canvas->SetPixel(x, y - 1, gr, gg, gb);
                    if (y + 1 < matrix_height) canvas->SetPixel(x, y + 1, gr, gg, gb);
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string StarFieldScene::get_name() const { return "starfield"; }

    void StarFieldScene::register_properties() {
        add_property(num_stars);
        add_property(speed);
        add_property(enable_twinkle);
        add_property(max_depth);
        add_property(colored_stars);
        add_property(streak_length);
        add_property(drifting_center);
    }

    std::unique_ptr<Scenes::Scene> StarFieldSceneWrapper::create() {
        return std::make_unique<StarFieldScene>();
    }
}
