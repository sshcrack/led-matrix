#include "KaleidoscopeGenesisScene.h"
#include <algorithm>

namespace AmbientScenes {
    KaleidoscopeGenesisScene::KaleidoscopeGenesisScene() : Scene(), rng(std::random_device{}()) {
    }

    void KaleidoscopeGenesisScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        hue_offset = 0;
        pattern_phase = 0;
    }

    void KaleidoscopeGenesisScene::hsl_to_rgb(float h, float s, float l, uint8_t &r, uint8_t &g, uint8_t &b) {
        while (h < 0) h += 360;
        while (h >= 360) h -= 360;

        float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = l - c / 2.0f;

        float rf, gf, bf;
        if (h < 60) {
            rf = c; gf = x; bf = 0;
        } else if (h < 120) {
            rf = x; gf = c; bf = 0;
        } else if (h < 180) {
            rf = 0; gf = c; bf = x;
        } else if (h < 240) {
            rf = 0; gf = x; bf = c;
        } else if (h < 300) {
            rf = x; gf = 0; bf = c;
        } else {
            rf = c; gf = 0; bf = x;
        }

        r = static_cast<uint8_t>((rf + m) * 255);
        g = static_cast<uint8_t>((gf + m) * 255);
        b = static_cast<uint8_t>((bf + m) * 255);
    }

    void KaleidoscopeGenesisScene::generate_pattern(int center_x, int center_y) {
        points.clear();

        int num_layers = 3;
        int sym = std::clamp(symmetry->get(), 3, 12);

        for (int layer = 0; layer < num_layers; layer++) {
            float radius = 10.0f + layer * 15.0f;
            int num_points = 4 + layer * 3;

            for (int i = 0; i < num_points; i++) {
                float base_angle = (2.0f * M_PI * i) / num_points;

                for (int s = 0; s < sym; s++) {
                    float angle = base_angle + (2.0f * M_PI * s) / sym + pattern_phase;
                    float x = center_x + radius * std::cos(angle);
                    float y = center_y + radius * std::sin(angle);

                    // Color based on layer and angle
                    float hue = std::fmod(hue_offset + (angle * 180.0f / M_PI) + (layer * 60.0f), 360.0f);
                    uint8_t r, g, b;
                    hsl_to_rgb(hue, 0.8f, 0.5f, r, g, b);

                    points.emplace_back(x, y, r, g, b);
                }
            }
        }
    }

    bool KaleidoscopeGenesisScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

        int center_x = matrix->width() / 2;
        int center_y = matrix->height() / 2;

        // Update pattern evolution
        hue_offset = std::fmod(hue_offset + evolution_speed->get() * 0.5f, 360.0f);
        pattern_phase += rotation_speed->get() * 0.02f;

        // Generate new pattern
        generate_pattern(center_x, center_y);

        // Draw points and connecting lines
        for (size_t i = 0; i < points.size(); i++) {
            const Point &p = points[i];
            int x = static_cast<int>(p.x);
            int y = static_cast<int>(p.y);

            if (x >= 0 && x < matrix->width() && y >= 0 && y < matrix->height()) {
                offscreen_canvas->SetPixel(x, y, p.r, p.g, p.b);

                // Connect to next point in same layer (optional lines)
                if (i + 1 < points.size()) {
                    const Point &next = points[i + 1];
                    int nx = static_cast<int>(next.x);
                    int ny = static_cast<int>(next.y);

                    // Simple line interpolation for connecting nearby points
                    float dx = nx - x;
                    float dy = ny - y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist < 20 && dist > 0) {
                        int steps = static_cast<int>(dist);
                        for (int step = 1; step < steps; step++) {
                            float t = static_cast<float>(step) / steps;
                            int lx = static_cast<int>(x + dx * t);
                            int ly = static_cast<int>(y + dy * t);

                            if (lx >= 0 && lx < matrix->width() && ly >= 0 && ly < matrix->height()) {
                                uint8_t intensity = static_cast<uint8_t>(255 * (1.0f - t * 0.5f));
                                offscreen_canvas->SetPixel(lx, ly,
                                    p.r * intensity / 255,
                                    p.g * intensity / 255,
                                    p.b * intensity / 255);
                            }
                        }
                    }
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string KaleidoscopeGenesisScene::get_name() const {
        return "kaleidoscope_genesis";
    }

    void KaleidoscopeGenesisScene::register_properties() {
        add_property(symmetry);
        add_property(rotation_speed);
        add_property(evolution_speed);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> KaleidoscopeGenesisSceneWrapper::create() {
        return {new KaleidoscopeGenesisScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<KaleidoscopeGenesisScene *>(scene);
        }};
    }
}
