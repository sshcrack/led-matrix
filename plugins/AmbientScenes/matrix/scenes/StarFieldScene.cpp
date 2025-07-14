#include "StarFieldScene.h"
#include <cmath>

namespace AmbientScenes {
    void StarFieldScene::Star::respawn(float max_depth) {
        x = (float) (rand() % 2000 - 1000) / 1000.0f;
        y = (float) (rand() % 2000 - 1000) / 1000.0f;
        z = max_depth;  // Start at maximum depth
    }

    void StarFieldScene::Star::update(float speed) {
        z -= speed;
    }

    StarFieldScene::StarFieldScene() :
            Scene(),
            gen(rd()) {
    }

    void StarFieldScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        stars.resize(num_stars->get());
        dis = std::uniform_real_distribution<>(0.0, 1.0);

        // Initialize stars at different depths
        for (auto &star: stars) {
            star.x = (float) (rand() % 2000 - 1000) / 1000.0f;
            star.y = (float) (rand() % 2000 - 1000) / 1000.0f;
            star.z = (float) (rand() % (int) (max_depth->get() * 1000)) / 1000.0f;
        }
    }

    bool StarFieldScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

        int center_x = matrix->width() / 2;
        int center_y = matrix->height() / 2;

        for (auto &star: stars) {
            star.update(speed->get());

            // Respawn star if it passes the viewer
            if (star.z <= 0.0f) {
                star.respawn(max_depth->get());
            }

            // Project 3D coordinates to 2D screen space with perspective division
            float perspective = 0.5f / star.z;
            int x = static_cast<int>(star.x * perspective * center_x + center_x);
            int y = static_cast<int>(star.y * perspective * center_y + center_y);

            // Calculate brightness based on z-position with non-linear falloff
            float depth_factor = (max_depth->get() - star.z) / max_depth->get();
            uint8_t brightness = static_cast<uint8_t>(255 * std::pow(depth_factor, 0.5f));

            // Add twinkle effect
            if (enable_twinkle) {
                brightness = static_cast<uint8_t>(brightness * (0.8f + 0.2f * dis(gen)));
            }

            // Draw star if it's within bounds
            if (x >= 0 && x < matrix->width() && y >= 0 && y < matrix->height()) {
                offscreen_canvas->SetPixel(x, y, brightness, brightness, brightness);

                // Add subtle glow for closer stars
                if (star.z < max_depth->get() * 0.3f) {
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

    void StarFieldScene::register_properties() {
        add_property(num_stars);
        add_property(speed);
        add_property(enable_twinkle);
        add_property(max_depth);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> StarFieldSceneWrapper::create() {
        return {new StarFieldScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<StarFieldScene *>(scene);
        }};
    }
}
