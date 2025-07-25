#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/wrappers.h"
#include "shared/matrix/utils/FrameTimer.h"
#include "shared/matrix/plugin/color.h"
#include "../AudioVisualizer.h"

namespace Scenes {
    class AudioSpectrumScene : public Scene {
    private:
        FrameTimer frameTimer;
        AudioVisualizer* plugin;
        uint32_t last_timestamp;
        
        // Properties for the scene
        PropertyPointer<int> bar_width = MAKE_PROPERTY_MINMAX("bar_width", int, 2, 1, 10);
        PropertyPointer<int> gap_width = MAKE_PROPERTY_MINMAX("gap_width", int, 1, 0, 5);
        PropertyPointer<bool> mirror_display = MAKE_PROPERTY("mirror_display", bool, true);
        PropertyPointer<bool> rainbow_colors = MAKE_PROPERTY("rainbow_colors", bool, true);
        //TODO: This may cause a memory leak
        PropertyPointer<Plugins::Color> base_color = MAKE_PROPERTY("base_color", Plugins::Color, Plugins::Color(0x00FF00)); // Default green
        PropertyPointer<bool> falling_dots = MAKE_PROPERTY("falling_dots", bool, true);
        PropertyPointer<float> dot_fall_speed = MAKE_PROPERTY_MINMAX("dot_fall_speed", float, 0.15f, 0.01f, 1.0f);
        PropertyPointer<int> display_mode = MAKE_PROPERTY_MINMAX("display_mode", int, 0, 0, 2); // 0=normal, 1=center-out, 2=edges-to-center
        
        std::vector<float> peak_positions;
        
        // Helper methods
        void initialize_if_needed(int num_bands);
        uint32_t get_bar_color(int band_index, float intensity, int num_bands) const;
        
    public:
        AudioSpectrumScene();
        ~AudioSpectrumScene() override = default;

        bool render(rgb_matrix::RGBMatrixBase *matrix) override;
        string get_name() const override;
        void register_properties() override;
        
        tmillis_t get_default_duration() override {
            return 20000;
        }
        
        int get_default_weight() override {
            return 5;
        }
    };

    class AudioSpectrumSceneWrapper : public Plugins::SceneWrapper {
    public:
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
    };
}
