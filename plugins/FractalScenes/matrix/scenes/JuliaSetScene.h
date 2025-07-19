#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <complex>

namespace Scenes {
    class JuliaSetSceneWrapper final : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scene, void (*)(Scene *)> create() override;
    };

    class JuliaSetScene final : public Scene {
    public:
        JuliaSetScene();
        ~JuliaSetScene() override = default;

        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;
        bool render(RGBMatrixBase *matrix) override;
        [[nodiscard]] string get_name() const override;

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 1;
        }
    protected:
        void register_properties() override;

    private:
        std::chrono::steady_clock::time_point last_update;
        float total_time = 0.0f;
        float animation_speed = 0.1f;
        
        // Julia set parameters
        std::complex<float> c = {-0.7, 0.27};
        
        // Properties
        PropertyPointer<float> zoom = MAKE_PROPERTY_MINMAX("zoom", float, 0.8f, 0.1f, 3.0f);
        PropertyPointer<float> move_speed = MAKE_PROPERTY_MINMAX("move_speed", float, 0.1f, 0.0f, 1.0f);
        PropertyPointer<int> max_iterations = MAKE_PROPERTY_MINMAX("max_iterations", int, 100, 10, 500);
        PropertyPointer<bool> animate_params = MAKE_PROPERTY("animate_params", bool, true);
        PropertyPointer<float> color_shift = MAKE_PROPERTY_MINMAX("color_shift", float, 0.0f, 0.0f, 1.0f);
        
        // Color mapping
        void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) const;
    };
}
