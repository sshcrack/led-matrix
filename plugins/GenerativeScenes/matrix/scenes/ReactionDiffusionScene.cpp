#include "ReactionDiffusionScene.h"
#include <cmath>
#include <algorithm>

namespace GenerativeScenes {

    ReactionDiffusionScene::ReactionDiffusionScene() : rng(std::random_device{}()) {
        set_target_fps(30);
    }

    void ReactionDiffusionScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *canvas) {
        Scene::initialize(matrix, canvas);
        u_cur.fill(1.0f);
        v_cur.fill(0.0f);
        seed_random_patch();
        step_count = 0;
        current_preset = 0;
        global_hue = 0.0f;
    }

    void ReactionDiffusionScene::seed_random_patch() {
        const int W = matrix_width;
        const int H = matrix_height;

        std::uniform_int_distribution<int> rx(5, W - 6);
        std::uniform_int_distribution<int> ry(5, H - 6);

        // Scatter several small square seeds of pure V across the canvas
        int num_seeds = 6 + static_cast<int>(rng() % 8);
        for (int s = 0; s < num_seeds; s++) {
            int cx = rx(rng);
            int cy = ry(rng);
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int x = cx + dx;
                    int y = cy + dy;
                    if (x >= 0 && x < W && y >= 0 && y < H) {
                        u_cur[y * W + x] = 0.0f;
                        v_cur[y * W + x] = 1.0f;
                    }
                }
            }
        }
    }

    void ReactionDiffusionScene::simulation_step(float F, float k) {
        const int W = matrix_width;
        const int H = matrix_height;

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                // Toroidal (wrap-around) boundary conditions
                const int xp = (x + 1) % W;
                const int xm = (x - 1 + W) % W;
                const int yp = (y + 1) % H;
                const int ym = (y - 1 + H) % H;

                const float u = u_cur[y * W + x];
                const float v = v_cur[y * W + x];

                const float lap_u = u_cur[y * W + xp] + u_cur[y * W + xm]
                                  + u_cur[yp * W + x] + u_cur[ym * W + x]
                                  - 4.0f * u;

                const float lap_v = v_cur[y * W + xp] + v_cur[y * W + xm]
                                  + v_cur[yp * W + x] + v_cur[ym * W + x]
                                  - 4.0f * v;

                const float uvv = u * v * v;

                u_nxt[y * W + x] = std::clamp(u + DT * (DU * lap_u - uvv + F * (1.0f - u)), 0.0f, 1.0f);
                v_nxt[y * W + x] = std::clamp(v + DT * (DV * lap_v + uvv - (F + k) * v),   0.0f, 1.0f);
            }
        }

        std::swap(u_cur, u_nxt);
        std::swap(v_cur, v_nxt);
    }

    // Maps V concentration + a slowly drifting hue offset to an RGB colour.
    // V≈0 (background) stays near-black; higher V values glow vividly.
    std::tuple<uint8_t, uint8_t, uint8_t> ReactionDiffusionScene::palette(float v, float hue_shift) {
        if (v < 0.04f) {
            return {0, 0, 12}; // near-black deep-blue background
        }

        // Remap to perceptually useful [0,1]; V peaks around 0.3–0.5 in most presets
        float t = std::min(v * 2.5f, 1.0f);
        t = t * t * (3.0f - 2.0f * t); // smoothstep for soft edge/core transition

        // Hue sweeps a vivid 160° arc of the colour wheel, anchored to the
        // slowly drifting global offset so the palette rotates over time.
        float hue = std::fmod(hue_shift + 0.55f - t * 0.45f, 1.0f);
        if (hue < 0.0f) hue += 1.0f;

        const float sat = 0.92f;
        const float val = 0.15f + t * 0.85f; // dim at outer rim, bright at core

        // HSV → RGB  (H expressed in [0,1])
        const float h6 = hue * 6.0f;
        const int   i  = static_cast<int>(h6);
        const float f  = h6 - static_cast<float>(i);
        const float p  = val * (1.0f - sat);
        const float q  = val * (1.0f - sat * f);
        const float tv = val * (1.0f - sat * (1.0f - f));

        float r, g, b;
        switch (i % 6) {
            case 0:  r = val; g = tv;  b = p;   break;
            case 1:  r = q;   g = val; b = p;   break;
            case 2:  r = p;   g = val; b = tv;  break;
            case 3:  r = p;   g = q;   b = val; break;
            case 4:  r = tv;  g = p;   b = val; break;
            default: r = val; g = p;   b = q;   break;
        }

        return {
            static_cast<uint8_t>(r * 255.0f),
            static_cast<uint8_t>(g * 255.0f),
            static_cast<uint8_t>(b * 255.0f)
        };
    }

    bool ReactionDiffusionScene::render(RGBMatrixBase *matrix) {
        const Preset &preset = PRESETS[current_preset];

        for (int s = 0; s < SIM_STEPS_PER_FRAME; s++) {
            simulation_step(preset.F, preset.k);
        }
        step_count += SIM_STEPS_PER_FRAME;

        // Cycle to the next preset and re-seed once enough steps have elapsed
        if (step_count >= STEPS_PER_PRESET) {
            current_preset = (current_preset + 1) % NUM_PRESETS;
            u_cur.fill(1.0f);
            v_cur.fill(0.0f);
            seed_random_patch();
            step_count = 0;
        }

        // Slowly rotate the colour palette
        global_hue = std::fmod(global_hue + 0.00025f, 1.0f);

        const int W = matrix_width;
        const int H = matrix_height;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                const float v = v_cur[y * W + x];
                auto [r, g, b] = palette(v, global_hue);
                offscreen_canvas->SetPixel(x, y, r, g, b);
            }
        }

        wait_until_next_frame();
        return true;
    }

    void ReactionDiffusionScene::register_properties() {
        // The scene manages all state internally; nothing user-configurable.
    }

} // namespace GenerativeScenes
