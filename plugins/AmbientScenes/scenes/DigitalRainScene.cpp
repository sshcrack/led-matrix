#include "DigitalRainScene.h"
#include <algorithm>
#include <cmath>

namespace AmbientScenes {
    DigitalRainScene::DigitalRainScene() : Scene(), gen(rd()) {}

    void DigitalRainScene::reset_drop(Drop& drop) {
        if (!matrix_width || !matrix_height) return;
        drop.x = dis_x(gen);
        drop.length = dis_length(gen);
        drop.y = -static_cast<float>(drop.length + (gen() % std::max(1, matrix_height / 2)));
        drop.depth = 0.45f + static_cast<float>(dis_speed(gen)) * 0.55f;
        drop.speed = base_speed->get() * (0.45f + drop.depth * 1.25f);
        drop.seed = gen();
    }

    void DigitalRainScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        matrix_brightness.assign(width, std::vector<float>(height, 0.0f));
        matrix_symbols.assign(width, std::vector<uint8_t>(height, 0));
        drops.resize(std::max(1, num_drops->get()));

        dis_speed = std::uniform_real_distribution<>(0.0, 1.0);
        dis_length = std::uniform_int_distribution<>(std::max(4, height / 14), std::max(6, height / 2));
        dis_x = std::uniform_int_distribution<>(0, std::max(0, width - 1));

        for (auto& drop : drops) {
            reset_drop(drop);
            drop.y = std::uniform_real_distribution<float>(-static_cast<float>(height), static_cast<float>(height))(gen);
        }
        simulation_tick = 0;
        simulation_accumulator = 0.0f;
        last_update = std::chrono::steady_clock::now();
    }

    void DigitalRainScene::draw_symbol(rgb_matrix::FrameCanvas* canvas, int x, int y, uint8_t symbol,
                                       uint8_t r, uint8_t g, uint8_t b) const {
        if (x < 0 || x >= matrix_width || y < 0 || y >= matrix_height) return;
        canvas->SetPixel(x, y, r, g, b);
        if (!symbol_mode->get()) return;

        // Tiny pseudo-glyph accents make the streams look like changing characters
        // while remaining readable on a dense physical matrix.
        const int side = (symbol & 1u) ? 1 : -1;
        if ((symbol & 2u) && x + side >= 0 && x + side < matrix_width)
            canvas->SetPixel(x + side, y, r / 2, g / 2, b / 2);
        if ((symbol & 4u) && y + 1 < matrix_height)
            canvas->SetPixel(x, y + 1, r / 3, g / 3, b / 3);
    }

    bool DigitalRainScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();

        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::min(0.25f, std::chrono::duration<float>(now - last_update).count());
        last_update = now;
        simulation_accumulator += elapsed;

        const std::size_t wanted_drops = static_cast<std::size_t>(std::max(1, num_drops->get()));
        if (drops.size() != wanted_drops) {
            const std::size_t old_size = drops.size();
            drops.resize(wanted_drops);
            for (std::size_t i = old_size; i < drops.size(); ++i) reset_drop(drops[i]);
        }

        int updates = 0;
        while (simulation_accumulator >= simulation_step && updates < 4) {
            simulation_accumulator -= simulation_step;
            ++updates;
            ++simulation_tick;

            // fade_factor historically described one 60 FPS frame. Exponentiation preserves
            // that visual decay while making it independent of the actual render frequency.
            const float fade_per_60hz_frame = std::clamp(fade_factor->get(), 0.0f, 0.999f);
            const float fade = std::pow(fade_per_60hz_frame, simulation_step * 60.0f);
            for (int x = 0; x < matrix_width; ++x) {
                for (int y = 0; y < matrix_height; ++y) {
                    matrix_brightness[x][y] *= fade;
                    if (matrix_brightness[x][y] < 0.015f) matrix_brightness[x][y] = 0.0f;
                }
            }

            for (auto& drop : drops) {
                const int old_y = static_cast<int>(drop.y);
                // Existing speed values were pixels per 60 Hz frame.
                drop.y += drop.speed * simulation_step * 60.0f;
                const int new_y = static_cast<int>(drop.y);

                for (int y = std::max(0, old_y); y <= new_y && y < matrix_height; ++y) {
                    if (y < 0) continue;
                    matrix_brightness[drop.x][y] = std::max(matrix_brightness[drop.x][y], drop.depth);
                    matrix_symbols[drop.x][y] = static_cast<uint8_t>((drop.seed + y * 17u + simulation_tick / 2u) & 7u);
                }

                if (glitch_effect->get() && (drop.seed + simulation_tick) % 97u == 0u && new_y >= 0 && new_y < matrix_height) {
                    const int gx = std::clamp(drop.x + ((drop.seed & 1u) ? 1 : -1), 0, matrix_width - 1);
                    matrix_brightness[gx][new_y] = drop.depth * 0.65f;
                    matrix_symbols[gx][new_y] = static_cast<uint8_t>(drop.seed & 7u);
                }

                if (drop.y - drop.length > matrix_height) reset_drop(drop);
            }
        }
        if (updates == 4) simulation_accumulator = 0.0f;

        const rgb_matrix::Color base = color->get();
        for (int x = 0; x < matrix_width; ++x) {
            for (int y = 0; y < matrix_height; ++y) {
                const float v = matrix_brightness[x][y];
                if (v <= 0.0f) continue;
                const float shaped = std::sqrt(v);
                draw_symbol(canvas, x, y, matrix_symbols[x][y],
                            static_cast<uint8_t>(base.r * shaped),
                            static_cast<uint8_t>(base.g * shaped),
                            static_cast<uint8_t>(base.b * shaped));
            }
        }

        for (const auto& drop : drops) {
            const int y = static_cast<int>(drop.y);
            if (y < 0 || y >= matrix_height) continue;
            canvas->SetPixel(drop.x, y, 235, 255, 245);
            if (y > 0) canvas->SetPixel(drop.x, y - 1, 120, 255, 175);
        }

        wait_until_next_frame();
        return true;
    }

    std::string DigitalRainScene::get_name() const { return "digitalrain"; }

    Scenes::SceneDescriptor DigitalRainScene::get_descriptor() const {
        auto d = Scene::get_descriptor();
        d.automatic_eligible = true;
        d.family = "digital-rain";
        d.tags = {"ambient", "geometric", "rain", "depth", "texture"};
        d.intensity = 0.46f;
        d.motion = 0.62f;
        d.music_affinity = 0.18f;
        d.performance_cost = 0.44f;
        d.variants = {
            {"drizzle", "Soft code rain", "Sparse slow trails for quiet ambient periods",
             {{"num_drops", 24}, {"base_speed", 0.62f}, {"fade_factor", 0.94f}, {"glitch_effect", false}},
             {"calm", "minimal", "depth"}, 0.24f, 0.34f, 0.08f, 0.36f},
            {"classic", "Digital rain", "Balanced falling symbols with subtle glitches",
             {{"num_drops", 42}, {"base_speed", 1.0f}, {"fade_factor", 0.90f}, {"symbol_mode", true}, {"glitch_effect", true}},
             {"ambient", "geometric"}, 0.48f, 0.62f, 0.16f, 0.44f},
            {"storm", "Data storm", "Dense fast rain for higher-energy ambient moments",
             {{"num_drops", 70}, {"base_speed", 1.65f}, {"fade_factor", 0.86f}, {"glitch_effect", true}},
             {"dense", "energetic", "geometric"}, 0.78f, 0.88f, 0.24f, 0.58f},
        };
        return d;
    }

    void DigitalRainScene::register_properties() {
        add_property(num_drops);
        add_property(base_speed);
        add_property(fade_factor);
        add_property(color);
        add_property(symbol_mode);
        add_property(glitch_effect);
    }

    std::unique_ptr<Scenes::Scene> DigitalRainSceneWrapper::create() {
        return std::make_unique<DigitalRainScene>();
    }
}
