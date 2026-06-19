#pragma once

#include "shared/matrix/Scene.h"
#include "shared/matrix/plugin/main.h"
#include <chrono>
#include <random>
#include <utility>
#include <vector>

namespace AmbientScenes {
    class SortingVisualizerScene : public Scenes::Scene {
    private:
        enum class Algorithm {
            BUBBLE_SORT,
            INSERTION_SORT,
            SELECTION_SORT,
            COCKTAIL_SORT,
            SHELL_SORT,
            QUICK_SORT,
            HEAP_SORT,
            COMB_SORT,
            GNOME_SORT,
            MERGE_SORT,
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
        int bubble_pass_end;
        int cocktail_start;
        int cocktail_end;
        bool cocktail_forward;
        int shell_gap;
        int shell_i;
        int shell_j;
        int shell_key;
        int heap_end;
        int heap_build_root;
        int heap_sift_root;
        bool heap_building;
        
        // Comb sort state
        int comb_gap;
        float comb_shrink;
        bool comb_swapped;
        int comb_idx;

        // Gnome sort state
        int gnome_pos;

        // Merge sort state (iterative bottom-up)
        int ms_width;
        int ms_left;
        int ms_mid;
        int ms_right;
        int ms_i;
        int ms_j;
        bool ms_merging;
        std::vector<int> ms_temp;
        
        // Quicksort stack
        std::vector<std::pair<int, int>> qs_stack;
        int qs_low, qs_high, qs_pivot_value, qs_pivot_index, qs_i, qs_j;
        bool qs_partitioning;

        PropertyPointer<int> delay_ms = MAKE_PROPERTY("delay_ms", int, 10);
        PropertyPointer<bool> rainbow_mode = MAKE_PROPERTY("rainbow_mode", bool, true);
        PropertyPointer<int> bar_gap = MAKE_PROPERTY("bar_gap", int, 0);
        PropertyPointer<rgb_matrix::Color> bar_color = MAKE_PROPERTY("bar_color", rgb_matrix::Color, rgb_matrix::Color(255, 255, 255));
        PropertyPointer<rgb_matrix::Color> solved_color = MAKE_PROPERTY("solved_color", rgb_matrix::Color, rgb_matrix::Color(64, 220, 96));

        void reset_array();
        void pick_next_algorithm();
        void step_sort();

        // Algorithm steps
        bool step_bubble();
        bool step_insertion();
        bool step_selection();
        bool step_cocktail();
        bool step_shell();
        bool step_quicksort();
        bool step_heap();
        bool step_comb();
        bool step_gnome();
        bool step_merge();

    public:
        explicit SortingVisualizerScene();
        ~SortingVisualizerScene() override = default;

        void register_properties() override;
        bool render(rgb_matrix::FrameCanvas *canvas) override;
        void initialize(int width, int height) override;

        tmillis_t get_default_duration() override { return 30000; }
        int get_default_weight() override { return 1; }
        [[nodiscard]] std::string get_name() const override;
    };

    class SortingVisualizerSceneWrapper : public Plugins::SceneWrapper {
        std::unique_ptr<Scenes::Scene> create() override;
    };
}
