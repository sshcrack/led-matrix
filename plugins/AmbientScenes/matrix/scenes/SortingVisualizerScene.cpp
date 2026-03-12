#include "SortingVisualizerScene.h"
#include <cmath>

namespace AmbientScenes {
    SortingVisualizerScene::SortingVisualizerScene() : Scene() {
        current_algorithm = Algorithm::BUBBLE_SORT;
        sort_phase = 0;
        delay_counter = 0;
    }

    void SortingVisualizerScene::hsl_to_rgb(float h, float s, float l, uint8_t& r, uint8_t& g, uint8_t& b) {
        float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = l - c / 2.0f;
        
        float r1 = 0, g1 = 0, b1 = 0;
        if (h >= 0 && h < 60) { r1 = c; g1 = x; b1 = 0; }
        else if (h >= 60 && h < 120) { r1 = x; g1 = c; b1 = 0; }
        else if (h >= 120 && h < 180) { r1 = 0; g1 = c; b1 = x; }
        else if (h >= 180 && h < 240) { r1 = 0; g1 = x; b1 = c; }
        else if (h >= 240 && h < 300) { r1 = x; g1 = 0; b1 = c; }
        else if (h >= 300 && h < 360) { r1 = c; g1 = 0; b1 = x; }
        
        r = static_cast<uint8_t>((r1 + m) * 255.0f);
        g = static_cast<uint8_t>((g1 + m) * 255.0f);
        b = static_cast<uint8_t>((b1 + m) * 255.0f);
    }

    void SortingVisualizerScene::reset_array() {
        int gap = std::max(0, bar_gap->get());
        int current_num_bars = (matrix_width + gap) / (1 + gap);
        if (current_num_bars < 1) current_num_bars = 1;

        array_data.clear();
        for (int k = 0; k < current_num_bars; ++k) {
            array_data.push_back(k + 1);
        }
        
        // Shuffle the array
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(array_data.begin(), array_data.end(), g);

        sort_phase = 1;
        access_indices.clear();
        delay_counter = 0;
        last_step_time = std::chrono::steady_clock::now();

        // Initialize algorithm state
        if (current_algorithm == Algorithm::BUBBLE_SORT) {
            i = 0; j = 0;
        } else if (current_algorithm == Algorithm::SELECTION_SORT) {
            i = 0; j = i + 1; min_idx = i;
        } else if (current_algorithm == Algorithm::INSERTION_SORT) {
            i = 1; key = array_data[i]; j = i - 1;
        } else if (current_algorithm == Algorithm::QUICK_SORT) {
            qs_stack.clear();
            qs_stack.push_back({0, (int)array_data.size() - 1});
            qs_partitioning = false;
        }
    }

    void SortingVisualizerScene::pick_next_algorithm() {
        int next_algo = (static_cast<int>(current_algorithm) + 1) % static_cast<int>(Algorithm::MAX_ALGORITHM);
        current_algorithm = static_cast<Algorithm>(next_algo);
        reset_array();
    }

    bool SortingVisualizerScene::step_bubble() {
        int n = array_data.size();
        if (i < n - 1) {
            if (j < n - i - 1) {
                access_indices = {j, j + 1};
                if (array_data[j] > array_data[j + 1]) {
                    std::swap(array_data[j], array_data[j + 1]);
                }
                j++;
            } else {
                j = 0;
                i++;
            }
            return false;
        }
        return true;
    }

    bool SortingVisualizerScene::step_selection() {
        int n = array_data.size();
        if (i < n - 1) {
            if (j < n) {
                access_indices = {min_idx, j};
                if (array_data[j] < array_data[min_idx]) {
                    min_idx = j;
                }
                j++;
            } else {
                access_indices = {i, min_idx};
                std::swap(array_data[min_idx], array_data[i]);
                i++;
                min_idx = i;
                j = i + 1;
            }
            return false;
        }
        return true;
    }

    bool SortingVisualizerScene::step_insertion() {
        int n = array_data.size();
        if (i < n) {
            access_indices = {j, i};
            if (j >= 0 && array_data[j] > key) {
                array_data[j + 1] = array_data[j];
                j = j - 1;
            } else {
                array_data[j + 1] = key;
                i++;
                if (i < n) {
                    key = array_data[i];
                    j = i - 1;
                }
            }
            return false;
        }
        return true;
    }

    bool SortingVisualizerScene::step_quicksort() {
        if (qs_stack.empty()) return true;

        if (!qs_partitioning) {
            auto range = qs_stack.back();
            qs_stack.pop_back();
            qs_low = range.first;
            qs_high = range.second;

            if (qs_low < qs_high) {
                qs_pivot = array_data[qs_high];
                qs_i = qs_low - 1;
                qs_j = qs_low;
                qs_partitioning = true;
            } else {
                return false;
            }
        } else {
            if (qs_j <= qs_high - 1) {
                access_indices = {qs_j, qs_high};
                if (array_data[qs_j] < qs_pivot) {
                    qs_i++;
                    std::swap(array_data[qs_i], array_data[qs_j]);
                }
                qs_j++;
            } else {
                std::swap(array_data[qs_i + 1], array_data[qs_high]);
                int pi = qs_i + 1;
                qs_partitioning = false;
                qs_stack.push_back({qs_low, pi - 1});
                qs_stack.push_back({pi + 1, qs_high});
            }
        }
        return false;
    }

    void SortingVisualizerScene::step_sort() {
        bool done = false;

        // Perform sorting steps. 
        int speed_multiplier = 1;
        if (delay_ms->get() <= 0) {
            if (current_algorithm == Algorithm::BUBBLE_SORT) speed_multiplier = 16;
            else if (current_algorithm == Algorithm::INSERTION_SORT) speed_multiplier = 8;
            else speed_multiplier = 4;
        }

        for (int step = 0; step < speed_multiplier && !done; ++step) {
            switch (current_algorithm) {
                case Algorithm::BUBBLE_SORT:     done = step_bubble(); break;
                case Algorithm::SELECTION_SORT:  done = step_selection(); break;
                case Algorithm::INSERTION_SORT:  done = step_insertion(); break;
                case Algorithm::QUICK_SORT:      done = step_quicksort(); break;
                case Algorithm::MAX_ALGORITHM:   break;
            }
        }

        if (done) {
            sort_phase = 2; // sorted wait phase
            access_indices.clear();
        }
    }

    void SortingVisualizerScene::initialize(RGBMatrixBase *matrix, rgb_matrix::FrameCanvas *l_offscreen_canvas) {
        Scene::initialize(matrix, l_offscreen_canvas);
        matrix_width = matrix->width();
        matrix_height = matrix->height();
        reset_array();
    }

    bool SortingVisualizerScene::render(RGBMatrixBase *matrix) {
        offscreen_canvas->Clear();

        int gap = std::max(0, bar_gap->get());
        int current_num_bars = (matrix_width + gap) / (1 + gap);
        if (current_num_bars < 1) current_num_bars = 1;

        if (array_data.size() != current_num_bars) {
            reset_array();
        }

        if (sort_phase == 1) {
            int delay = delay_ms->get();
            if (delay > 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_step_time).count();
                if (elapsed >= delay) {
                    step_sort();
                    last_step_time = now;
                }
            } else {
                step_sort();
            }
            delay_counter = 0;
        } else if (sort_phase == 2) {
            delay_counter++;
            if (delay_counter >= 100) { // wait for some frames
                pick_next_algorithm();
            }
        }

        for (int x = 0; x < array_data.size(); ++x) {
            int val = array_data[x];
            
            // Normalize val to height
            int bar_height = (val * matrix_height) / array_data.size();
            
            uint8_t r = 255, g = 255, b = 255;
            
            bool is_access = std::find(access_indices.begin(), access_indices.end(), x) != access_indices.end();
            
            if (is_access && sort_phase == 1) {
                r = 255; g = 0; b = 0;
            } else if (rainbow_mode->get()) {
                float hue = (float)val / array_data.size() * 360.0f;
                hsl_to_rgb(hue, 1.0f, 0.5f, r, g, b);
            } else {
                if (sort_phase == 2) {
                    r = 0; g = 255; b = 0;
                } else {
                    auto col = bar_color->get();
                    r = col.r; g = col.g; b = col.b;
                }
            }

            int screen_x = x * (1 + gap);
            for (int y = matrix_height - bar_height; y < matrix_height; ++y) {
                if (screen_x < matrix_width) {
                    offscreen_canvas->SetPixel(screen_x, y, r, g, b);
                }
            }
        }

        wait_until_next_frame();
        return true;
    }

    std::string SortingVisualizerScene::get_name() const {
        return "sorting-visualizer";
    }

    void SortingVisualizerScene::register_properties() {
        add_property(delay_ms);
        add_property(rainbow_mode);
        add_property(bar_gap);
        add_property(bar_color);
    }

    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> SortingVisualizerSceneWrapper::create() {
        return {new SortingVisualizerScene(), [](Scenes::Scene *scene) {
            delete dynamic_cast<SortingVisualizerScene *>(scene);
        }};
    }
}
