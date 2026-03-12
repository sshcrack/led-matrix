#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>

namespace AmbientScenes {
    class FallingSandScene : public Scenes::Scene {
    private:
        std::vector<uint32_t> grid; // 0 for empty, otherwise RGB packed (R << 16 | G << 8 | B)
        std::vector<uint32_t> next_grid; 

        PropertyPointer<int> spawn_rate = MAKE_PROPERTY("spawn_rate", int, 5);
        PropertyPointer<int> hue_speed = MAKE_PROPERTY("hue_speed", int, 2);
        PropertyPointer<int> max_sand = MAKE_PROPERTY("max_sand", int, 10000); 

        float current_hue = 0.0f;
        int sand_count = 0;
        int spawner_x = 64;
        int spawner_dir = 1;

        void hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b);

        uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
            return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
        }
        
        void unpack_color(uint32_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
            r = (c >> 16) & 0xFF;
            g = (c >> 8) & 0xFF;
            b = c & 0xFF;
        }

    public:
        explicit FallingSandScene();
        ~FallingSandScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 60000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class FallingSandSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
