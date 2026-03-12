#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <vector>
#include <random>
#include <chrono>

namespace AmbientScenes {
    class SortingVisualizerScene : public Scenes::Scene {
    private:
        enum class Algorithm {
            BUBBLE_SORT,
            INSERTION_SORT,
            SELECTION_SORT,
            QUICK_SORT,
            MAX_ALGORITHM
        };

        std::vector<int> array_data;
        std::vector<int> access_indices; // Indices currently being accessed/compared
        Algorithm current_algorithm;
        int sort_phase; // 0 = unsorting, 1 = sorting, 2 = sorted (wait), 3 = completed
        int delay_counter;
        std::chrono::time_point<std::chrono::steady_clock> last_step_time;
        
        // Sorting state variables
        int i, j;
        int min_idx;
        int key;
        
        // Quicksort stack
        std::vector<std::pair<int, int>> qs_stack;
        int qs_low, qs_high, qs_pivot, qs_i, qs_j;
        bool qs_partitioning;

        PropertyPointer<int> delay_ms = MAKE_PROPERTY("delay_ms", int, 10);
        PropertyPointer<bool> rainbow_mode = MAKE_PROPERTY("rainbow_mode", bool, true);
        PropertyPointer<int> bar_gap = MAKE_PROPERTY("bar_gap", int, 0);
        PropertyPointer<rgb_matrix::Color> bar_color = MAKE_PROPERTY("bar_color", rgb_matrix::Color, rgb_matrix::Color(255, 255, 255));

        // HSL to RGB conversion helper
        void hsl_to_rgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);

        void reset_array();
        void pick_next_algorithm();
        void step_sort();

        // Algorithm steps
        bool step_bubble();
        bool step_insertion();
        bool step_selection();
        bool step_quicksort();

    public:
        explicit SortingVisualizerScene();
        ~SortingVisualizerScene() override = default;

        void register_properties() override;
        bool render(RGBMatrixBase *matrix) override;
        void initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) override;

        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;

        using Scene::Scene;
    };

    class SortingVisualizerSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create();
    };
}
