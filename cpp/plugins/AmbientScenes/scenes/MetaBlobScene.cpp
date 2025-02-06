#include "MetaBlobScene.h"
#include <cmath>

namespace Scenes {
    float MetaBlobScene::rand_sin(int i) const {
        return std::sin(float(i) * 1.64f);
    }

    MetaBlobScene::Blob MetaBlobScene::get_blob(rgb_matrix::RGBMatrix *matrix, int i, float time) const {
        float x = 0.5f + 0.1f * rand_sin(i);
        float y = 0.5f + 0.1f * rand_sin(i + 42);
        
        x += move_range * std::sin(speed * time * rand_sin(i + 2)) * rand_sin(i + 56);
        y += move_range * -std::sin(speed * time) * rand_sin(i * 9);
        
        float radius = 0.1f * std::abs(rand_sin(i + 3));
        
        // Convert normalized coordinates to matrix space
        return Blob(
            x * matrix->width(),
            y * matrix->height(),
            radius * matrix->width()
        );
    }

    float MetaBlobScene::calculate_field(float x, float y, const Blob& blob) const {
        float dx = x - blob.x;
        float dy = y - blob.y;
        float dist = std::max(std::sqrt(dx*dx + dy*dy) + blob.radius/2.0f, 0.0f);
        float tmp = dist * dist;
        return 1.0f / (tmp * tmp);
    }

    MetaBlobScene::MetaBlobScene(const nlohmann::json &config)
        : Scene(config), time(0.0f) {
        num_blobs = config.value("num_blobs", 10);
        threshold = config.value("threshold", 0.0003f);  // Converted from shader's 3000
        speed = config.value("speed", 0.25f);
        move_range = config.value("move_range", 0.5f);
        // Slower default color speed (one full cycle every 30 seconds)
        color_speed = config.value("color_speed", 0.033f);
    }

    void MetaBlobScene::initialize(rgb_matrix::RGBMatrix *matrix) {
        Scene::initialize(matrix);
        blobs.reserve(num_blobs);
    }

    bool MetaBlobScene::render(rgb_matrix::RGBMatrix *matrix) {
        offscreen_canvas->Clear();
        
        // Update blob positions
        blobs.clear();
        for (int i = 0; i < num_blobs; i++) {
            blobs.push_back(get_blob(matrix, i, time));
        }

        // Calculate base color from time (use color_speed to control rate of change)
        float hue = std::fmod(time * color_speed, 1.0f);
        
        // Convert hue to RGB
        auto hue_to_rgb = [](float h) {
            h = std::fmod(h, 1.0f);
            if (h < 0) h += 1.0f;
            h *= 6.0f;
            
            int i = static_cast<int>(h);
            float f = h - i;
            
            switch (i) {
                case 0: return std::make_tuple(static_cast<uint8_t>(255), static_cast<uint8_t>(255 * f), static_cast<uint8_t>(0));
                case 1: return std::make_tuple(static_cast<uint8_t>(255 * (1 - f)), static_cast<uint8_t>(255), static_cast<uint8_t>(0));
                case 2: return std::make_tuple(static_cast<uint8_t>(0), static_cast<uint8_t>(255), static_cast<uint8_t>(255 * f));
                case 3: return std::make_tuple(static_cast<uint8_t>(0), static_cast<uint8_t>(255 * (1 - f)), static_cast<uint8_t>(255));
                case 4: return std::make_tuple(static_cast<uint8_t>(255 * f), static_cast<uint8_t>(0), static_cast<uint8_t>(255));
                case 5: return std::make_tuple(static_cast<uint8_t>(255), static_cast<uint8_t>(0), static_cast<uint8_t>(255 * (1 - f)));
                default: return std::make_tuple(static_cast<uint8_t>(255), static_cast<uint8_t>(0), static_cast<uint8_t>(0));
            }
        };

        // Render metaballs
        for (int y = 0; y < matrix->height(); y++) {
            for (int x = 0; x < matrix->width(); x++) {
                float dist_sum = 0.0f;
                for (const auto& blob : blobs) {
                    dist_sum += calculate_field(x, y, blob);
                }

                if (dist_sum > threshold) {
                    // Mix between center and edge colors
                    float t = std::min(1.0f, (dist_sum - threshold) / threshold);
                    t = t * t * (3.0f - 2.0f * t); // Smooth step
                    
                    // Get base color and edge color (90 degrees shifted hue)
                    auto [r1, g1, b1] = hue_to_rgb(hue);
                    auto [r2, g2, b2] = hue_to_rgb(hue + 0.25f);
                    
                    // Mix colors based on field strength
                    uint8_t r = static_cast<uint8_t>(r1 * t + r2 * (1 - t));
                    uint8_t g = static_cast<uint8_t>(g1 * t + g2 * (1 - t));
                    uint8_t b = static_cast<uint8_t>(b1 * t + b2 * (1 - t));
                    
                    offscreen_canvas->SetPixel(x, y, r, g, b);
                }
                // Black background - no else clause needed as Clear() sets black
            }
        }

        time += 1.0f / 60.0f;
        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
        return true;
    }

    std::string MetaBlobScene::get_name() const {
        return "metablob";
    }

    Scenes::Scene *MetaBlobSceneWrapper::create_default() {
        return new MetaBlobScene(Scene::create_default(3, 10 * 1000));
    }

    Scenes::Scene *MetaBlobSceneWrapper::from_json(const nlohmann::json &args) {
        return new MetaBlobScene(args);
    }
}
