#pragma once

#include "Scene.h"
#include "plugin/main.h"
#include <vector>
#include <random>

namespace AmbientScenes {
class FireScene : public Scenes::Scene {
        int width = 0;
        int height = 0;
        float time = 0.0f;  // Simulation time
        std::vector<std::vector<int>> fire_pixels;

        // Helper methods for shader simulation
        static float hash12(float x, float y);

        [[nodiscard]] static float linear_noise(float x, float y);

        [[nodiscard]] float fbm(float x, float y) const;

        PropertyPointer<float> frames_per_second = MAKE_PROPERTY("fps", float, 30.0f);
        [[maybe_unused]] float update_delay{};      // Delay between updates in seconds
        float accumulated_time;

        std::chrono::steady_clock::time_point last_update;

        void render_fire(rgb_matrix::Canvas *canvas);

    public:
        explicit FireScene();
        ~FireScene() override = default;

        bool render(RGBMatrixBase *matrix) override;

        void initialize(RGBMatrixBase *matrix, FrameCanvas *l_offscreen_canvas) override;

        [[nodiscard]] string get_name() const override;

        void register_properties() override;
        void load_properties(const nlohmann::json &j) override;
    };

    class FireSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}
