#include "NeonTunnelScene.h"
#include <cmath>

namespace AmbientScenes {
    NeonTunnelScene::NeonTunnelScene() : Scene() {
    }

    void NeonTunnelScene::hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
        float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = l - c / 2.0f;
        
        float r1 = 0, g1 = 0, b1 = 0;
        if (h >= 0 && h < 60) { r1 = c; g1 = x; b1 = 0; }
        else if (h >= 60 && h < 120) { r1 = x; g1 = c; b1 = 0; }
        else if (h >= 120 && h < 180) { r1 = 0; g1 = c; b1 = x; }
        else if (h >= 180 && h < 240) { r1 = 0; g1 = x; b1 = c; }
        else if (h >= 240 && h < 300) { r1 = x; g1 = 0; b1 = c; }
        else if (h >= 300 && h < 360) { r1 = c; g1 = 0; b1 = x; }
        
        r = static_cast<uint8_t>((r1 + m) * 255.0f);
        g = static_cast<uint8_t>((g1 + m) * 255.0f);
        b = static_cast<uint8_t>((b1 + m) * 255.0f);
    }

    void NeonTunnelScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        time_counter = 0.0f;
    }

    bool NeonTunnelScene::render(rgb_matrix::FrameCanvas *canvas) {
        time_counter += 0.05f;

        float center_x = matrix_width / 2.0f;
        float center_y = matrix_height / 2.0f;

        // Animate the center point slightly to make the effect loop / swing around
        float osc_x = center_x + std::sin(time_counter * 0.5f) * (matrix_width / 4.0f);
        float osc_y = center_y + std::cos(time_counter * 0.7f) * (matrix_height / 4.0f);

        for (int y = 0; y < matrix_height; ++y) {
            for (int x = 0; x < matrix_width; ++x) {
                float dx = x - osc_x;
                float dy = y - osc_y;
                float distance = std::sqrt(dx * dx + dy * dy);
                if (distance == 0) distance = 0.001f;

                float angle = std::atan2(dy, dx); // from -PI to PI

                // Generate U and V coordinates based on distance and angle
                float v_u = (distance_factor->get() / distance) + (speed->get() * time_counter);
                float v_v = (angle * angle_factor->get() / M_PI) + (std::sin(time_counter) * 2.0f);

                // Create XOR texture pattern based on U and V mapping
                int tex_x = (int)std::round(std::abs(v_u * 32.0f)) % 256;
                int tex_y = (int)std::round(std::abs(v_v * 32.0f)) % 256;
                
                int pattern = tex_x ^ tex_y;
                
                // Dim down the color based on depth/distance
                float depth_shade = 1.0f - (distance / (matrix_width * 1.5f));
                if (depth_shade < 0) depth_shade = 0.0f;

                // Color hue changes over time and depth
                float hue = std::fmod((time_counter * hue_shift_speed->get() * 50.0f) + (distance * 2.0f), 360.0f);

                // Checkerboard effect
                float lightness = (pattern > 128) ? (0.5f * depth_shade) : 0.00f;

                uint8_t r, g, b;
                hsl_to_rgb(hue, 1.0f, lightness, r, g, b);

                canvas->SetPixel(x, y, r, g, b);
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string NeonTunnelScene::get_name() const {
        return "neontunnel";
    }

    void NeonTunnelScene::register_properties() {
        add_property(speed);
        add_property(distance_factor);
        add_property(angle_factor);
        add_property(hue_shift_speed);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> NeonTunnelSceneWrapper::create() {
        return {new NeonTunnelScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<NeonTunnelScene *>(scene);
        }};
    }
}
