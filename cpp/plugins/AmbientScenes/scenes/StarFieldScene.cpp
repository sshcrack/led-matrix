#include "StarFieldScene.h"
#include <cmath>

namespace Scenes {
    void StarFieldScene::Star::respawn(float max_depth) {
        x = (float)(rand() % 2000 - 1000) / 1000.0f;
        y = (float)(rand() % 2000 - 1000) / 1000.0f;
        z = max_depth;  // Start at maximum depth
    }

    void StarFieldScene::Star::update(float speed) {
        z -= speed;
    }

    StarFieldScene::StarFieldScene(const nlohmann::json &config) :
            Scene(config),
            gen(rd()) {
        num_stars = config.value("num_stars", 50);
        speed = config.value("speed", 0.02f);
        enable_twinkle = config.value("enable_twinkle", true);
        max_depth = config.value("max_depth", 3.0f);
    }

    void StarFieldScene::initialize(rgb_matrix::RGBMatrix *matrix) {
        Scene::initialize(matrix);
        stars.resize(num_stars);
        dis = std::uniform_real_distribution<>(0.0, 1.0);
        
        // Initialize stars at different depths
        for (auto& star : stars) {
            star.x = (float)(rand() % 2000 - 1000) / 1000.0f;
            star.y = (float)(rand() % 2000 - 1000) / 1000.0f;
            star.z = (float)(rand() % (int)(max_depth * 1000)) / 1000.0f;
        }
    }

    bool StarFieldScene::render(rgb_matrix::RGBMatrix *matrix) {
        offscreen_canvas->Clear();

        int center_x = matrix->width() / 2;
        int center_y = matrix->height() / 2;

        for (auto& star : stars) {
            star.update(speed);
            
            // Respawn star if it passes the viewer
            if (star.z <= 0.0f) {
                star.respawn(max_depth);
            }

            // Project 3D coordinates to 2D screen space with perspective division
            float perspective = 0.5f / star.z;
            int x = static_cast<int>(star.x * perspective * center_x + center_x);
            int y = static_cast<int>(star.y * perspective * center_y + center_y);

            // Calculate brightness based on z-position with non-linear falloff
            float depth_factor = (max_depth - star.z) / max_depth;
            uint8_t brightness = static_cast<uint8_t>(255 * std::pow(depth_factor, 0.5f));

            // Add twinkle effect
            if (enable_twinkle) {
                brightness = static_cast<uint8_t>(brightness * (0.8f + 0.2f * dis(gen)));
            }

            // Draw star if it's within bounds
            if (x >= 0 && x < matrix->width() && y >= 0 && y < matrix->height()) {
                offscreen_canvas->SetPixel(x, y, brightness, brightness, brightness);
                
                // Add subtle glow for closer stars
                if (star.z < max_depth * 0.3f) {
                    uint8_t glow = brightness / 4;
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dy = -1; dy <= 1; dy++) {
                            if (dx == 0 && dy == 0) continue;
                            int gx = x + dx;
                            int gy = y + dy;
                            if (gx >= 0 && gx < matrix->width() && gy >= 0 && gy < matrix->height()) {
                                offscreen_canvas->SetPixel(gx, gy, glow, glow, glow);
                            }
                        }
                    }
                }
            }
        }

        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
        return true;
    }

    std::string StarFieldScene::get_name() const {
        return "starfield";
    }

    Scenes::Scene *StarFieldSceneWrapper::create_default() {
        return new StarFieldScene(Scene::create_default(3, 10 * 1000));
    }

    Scenes::Scene *StarFieldSceneWrapper::from_json(const nlohmann::json &args) {
        return new StarFieldScene(args);
    }
}
