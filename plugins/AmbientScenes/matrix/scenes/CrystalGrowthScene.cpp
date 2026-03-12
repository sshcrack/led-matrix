#include "CrystalGrowthScene.h"
#include <cmath>

namespace AmbientScenes {
    CrystalGrowthScene::CrystalGrowthScene() : Scene(), rng(std::random_device{}()) {
    }

    void CrystalGrowthScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        crystal.clear();
        frontier.clear();
        growth_frame = 0;

        // Randomize color
        std::uniform_int_distribution<> dis(100, 255);
        base_b = dis(rng);
        base_g = dis(rng);
        base_r = dis(rng);

        // Start with center seed
        int center_x = matrix->width() / 2;
        int center_y = matrix->height() / 2;
        crystal.insert({center_x, center_y});

        // Initialize frontier with neighbors
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;
                frontier.insert({center_x + dx, center_y + dy});
            }
        }
    }

    void CrystalGrowthScene::add_to_frontier(int x, int y) {
        Cell cell{x, y};
        if (crystal.find(cell) == crystal.end()) {
            frontier.insert(cell);
        }
    }

    void CrystalGrowthScene::apply_symmetry(int x, int y, int center_x, int center_y) {
        if (!symmetric_growth->get()) return;

        // 6-fold rotational symmetry
        for (int i = 0; i < 6; i++) {
            float angle = (2.0f * M_PI * i) / 6.0f;
            float cos_a = std::cos(angle);
            float sin_a = std::sin(angle);

            int dx = x - center_x;
            int dy = y - center_y;

            int new_x = static_cast<int>(center_x + dx * cos_a - dy * sin_a);
            int new_y = static_cast<int>(center_y + dx * sin_a + dy * cos_a);

            crystal.insert({new_x, new_y});

            // Update frontier
            for (int ddx = -1; ddx <= 1; ddx++) {
                for (int ddy = -1; ddy <= 1; ddy++) {
                    if (ddx == 0 && ddy == 0) continue;
                    add_to_frontier(new_x + ddx, new_y + ddy);
                }
            }
        }
    }

    void CrystalGrowthScene::grow_crystal() {
        if (frontier.empty()) return;

        // Grow multiple cells per frame
        int grow_count = std::clamp(growth_speed->get(), 1, 10);
        int center_x = 64;
        int center_y = 64;

        for (int g = 0; g < grow_count && !frontier.empty(); g++) {
            std::uniform_int_distribution<> dis(0, frontier.size() - 1);
            int idx = dis(rng);

            auto it = frontier.begin();
            std::advance(it, idx);
            Cell chosen = *it;
            frontier.erase(it);

            crystal.insert(chosen);

            if (symmetric_growth->get()) {
                apply_symmetry(chosen.x, chosen.y, center_x, center_y);
            }

            // Add neighbors to frontier
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0) continue;
                    add_to_frontier(chosen.x + dx, chosen.y + dy);
                }
            }
        }
    }

    bool CrystalGrowthScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

        // Grow crystal
        if (growth_frame % 2 == 0) {
            grow_crystal();
        }
        growth_frame++;

        // Reset if crystal gets too large
        if (crystal.size() > 1000) {
            initialize(matrix, offscreen_canvas);
        }

        int center_x = matrix->width() / 2;
        int center_y = matrix->height() / 2;

        // Draw crystal cells with color gradient
        for (const auto &cell : crystal) {
            if (cell.x >= 0 && cell.x < matrix->width() && cell.y >= 0 && cell.y < matrix->height()) {
                float dx = cell.x - center_x;
                float dy = cell.y - center_y;
                float dist = std::sqrt(dx * dx + dy * dy);

                // Color varies with distance from center
                uint8_t intensity = static_cast<uint8_t>(200 + 55 * std::sin(dist * 0.2f));
                uint8_t r = static_cast<uint8_t>(base_r * intensity / 255);
                uint8_t g = static_cast<uint8_t>(base_g * intensity / 255);
                uint8_t b = static_cast<uint8_t>(base_b * intensity / 255);

                offscreen_canvas->SetPixel(cell.x, cell.y, r, g, b);
            }
        }

        // Optional: draw frontier
        if (crystal.size() < 200) {
            for (const auto &cell : frontier) {
                if (cell.x >= 0 && cell.x < matrix->width() && cell.y >= 0 && cell.y < matrix->height()) {
                    offscreen_canvas->SetPixel(cell.x, cell.y, base_r / 3, base_g / 3, base_b / 3);
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string CrystalGrowthScene::get_name() const {
        return "crystal_growth";
    }

    void CrystalGrowthScene::register_properties() {
        add_property(growth_speed);
        add_property(symmetric_growth);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> CrystalGrowthSceneWrapper::create() {
        return {new CrystalGrowthScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<CrystalGrowthScene *>(scene);
        }};
    }
}
