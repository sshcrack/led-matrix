#pragma once

#include "shared/matrix/Scene.h.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <vector>

namespace Scenes {
    class WavePatternSceneWrapper final : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scene, void (*)(Scene *)> create() override;
    };
    
    class WavePatternScene final : public Scene {
    public:
        WavePatternScene();
        ~WavePatternScene() override = default;
        
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;
        bool render(RGBMatrixBase *matrix) override;
        string get_name() const override;

        tmillis_t get_default_duration() override {
            return 20000;
        }

        int get_default_weight() override {
            return 1;
        }
    protected:
        void register_properties() override;
        void load_properties(const json &j) override;
        
    private:
        std::chrono::steady_clock::time_point last_update;
        float total_time = 0.0f;
        
        // Wave parameters
        struct Wave {
            float amplitude;
            float frequency;
            float phase_speed;
            float phase_offset;
        };
        
        std::vector<Wave> waves;
        
        // Properties
        PropertyPointer<int> num_waves = MAKE_PROPERTY_MINMAX("num_waves", int, 3, 1, 10);
        PropertyPointer<float> speed = MAKE_PROPERTY_MINMAX("speed", float, 1.0f, 0.1f, 5.0f);
        PropertyPointer<float> color_speed = MAKE_PROPERTY_MINMAX("color_speed", float, 0.5f, 0.0f, 2.0f);
        PropertyPointer<bool> rainbow_mode = MAKE_PROPERTY("rainbow_mode", bool, true);
        PropertyPointer<float> wave_height = MAKE_PROPERTY_MINMAX("wave_height", float, 1.0f, 0.1f, 3.0f);
        
        // Initialize waves based on settings
        void init_waves();
        
        // Color mapping
        void hsv_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) const;
    };
}
