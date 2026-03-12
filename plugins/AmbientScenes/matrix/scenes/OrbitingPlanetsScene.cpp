#include "OrbitingPlanetsScene.h"
#include <cmath>

namespace AmbientScenes {
    OrbitingPlanetsScene::OrbitingPlanetsScene() : Scene() {
        // Initialize planets with different orbital parameters
        planets.emplace_back(15.0f, 0.01f, 1.5f, 255, 100, 100);    // Red planet
        planets.emplace_back(25.0f, 0.007f, 2.0f, 100, 150, 255);   // Blue planet
        planets.emplace_back(35.0f, 0.005f, 1.2f, 200, 255, 100);   // Green planet
        planets.emplace_back(45.0f, 0.003f, 1.8f, 255, 200, 100);   // Orange planet
    }

    void OrbitingPlanetsScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        time_elapsed = 0;
    }

    bool OrbitingPlanetsScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

        int center_x = matrix->width() / 2;
        int center_y = matrix->height() / 2;

        // Update time
        time_elapsed += rotation_speed->get() * 0.1f;

        // Draw central star
        offscreen_canvas->SetPixel(center_x, center_y, 255, 255, 200);
        offscreen_canvas->SetPixel(center_x + 1, center_y, 255, 255, 100);
        offscreen_canvas->SetPixel(center_x - 1, center_y, 255, 255, 100);
        offscreen_canvas->SetPixel(center_x, center_y + 1, 255, 255, 100);
        offscreen_canvas->SetPixel(center_x, center_y - 1, 255, 255, 100);

        // Draw orbital paths and planets
        for (auto &planet : planets) {
            // Update angle
            planet.angle = time_elapsed * planet.orbital_speed;

            // Draw orbital path as circle
            if (!show_trails->get()) {
                int segments = 32;
                for (int i = 0; i < segments; i++) {
                    float angle1 = (2.0f * M_PI * i) / segments;
                    float angle2 = (2.0f * M_PI * (i + 1)) / segments;

                    int x1 = static_cast<int>(center_x + planet.orbital_radius * std::cos(angle1));
                    int y1 = static_cast<int>(center_y + planet.orbital_radius * std::sin(angle1));
                    int x2 = static_cast<int>(center_x + planet.orbital_radius * std::cos(angle2));
                    int y2 = static_cast<int>(center_y + planet.orbital_radius * std::sin(angle2));

                    // Bresenham-like line drawing for orbit circles (simple version)
                    if (x1 >= 0 && x1 < matrix->width() && y1 >= 0 && y1 < matrix->height()) {
                        offscreen_canvas->SetPixel(x1, y1, planet.r / 5, planet.g / 5, planet.b / 5);
                    }
                }
            }

            // Calculate planet position
            float px = center_x + planet.orbital_radius * std::cos(planet.angle);
            float py = center_y + planet.orbital_radius * std::sin(planet.angle);

            int planet_x = static_cast<int>(px);
            int planet_y = static_cast<int>(py);

            // Draw planet
            if (planet_x >= 0 && planet_x < matrix->width() && planet_y >= 0 && planet_y < matrix->height()) {
                offscreen_canvas->SetPixel(planet_x, planet_y, planet.r, planet.g, planet.b);

                // Glow effect
                int glow_size = static_cast<int>(planet.size);
                for (int dx = -glow_size; dx <= glow_size; dx++) {
                    for (int dy = -glow_size; dy <= glow_size; dy++) {
                        int gx = planet_x + dx;
                        int gy = planet_y + dy;
                        if (gx >= 0 && gx < matrix->width() && gy >= 0 && gy < matrix->height()) {
                            float dist = std::sqrt(dx * dx + dy * dy);
                            if (dist > 0 && dist <= glow_size) {
                                uint8_t glow_intensity = static_cast<uint8_t>(100 * (1.0f - dist / glow_size) / 3);
                                offscreen_canvas->SetPixel(gx, gy,
                                    planet.r * glow_intensity / 255,
                                    planet.g * glow_intensity / 255,
                                    planet.b * glow_intensity / 255);
                            }
                        }
                    }
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string OrbitingPlanetsScene::get_name() const {
        return "orbiting_planets";
    }

    void OrbitingPlanetsScene::register_properties() {
        add_property(rotation_speed);
        add_property(show_trails);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> OrbitingPlanetsSceneWrapper::create() {
        return {new OrbitingPlanetsScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<OrbitingPlanetsScene *>(scene);
        }};
    }
}
