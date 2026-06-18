#include "NeonTunnelScene.h"
#include <shared/matrix/utils/color.h>
#include <cmath>

namespace AmbientScenes {
    NeonTunnelScene::NeonTunnelScene() : Scene() {
    }

    void NeonTunnelScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        time_counter = 0.0f;
    }

    bool NeonTunnelScene::render(rgb_matrix::FrameCanvas *canvas) {
        time_counter += 1.0f / static_cast<float>(get_target_fps()) * 3.0f;

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
                color::hsl_to_rgb(hue, 1.0f, lightness, r, g, b);

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

    std::unique_ptr<Scenes::Scene> NeonTunnelSceneWrapper::create() {
        return std::make_unique<NeonTunnelScene>();
    }
}
