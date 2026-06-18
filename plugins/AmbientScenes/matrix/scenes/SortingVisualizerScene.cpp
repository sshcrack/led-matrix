#include "SortingVisualizerScene.h"
#include <shared/matrix/utils/color.h>

#include <algorithm>
#include <cmath>

namespace AmbientScenes {
    namespace {
        const rgb_matrix::Color kAccessColor(255, 64, 64);
        const rgb_matrix::Color kSortedColor(64, 220, 96);
        const rgb_matrix::Color kPivotColor(255, 196, 64);
        const rgb_matrix::Color kPartitionLowColor(64, 192, 255);
        const rgb_matrix::Color kPartitionHighColor(196, 96, 255);
        const rgb_matrix::Color kPartitionLeftColor(72, 136, 255);
        const rgb_matrix::Color kPartitionRightColor(154, 92, 255);
        const rgb_matrix::Color kHeapRootColor(255, 168, 64);
        const rgb_matrix::Color kHeapBuildColor(96, 176, 255);
        const rgb_matrix::Color kKeyColor(255, 255, 255);

        bool contains_index(const std::vector<int> &indices, int index) {
            return std::find(indices.begin(), indices.end(), index) != indices.end();
        }
    }

    SortingVisualizerScene::SortingVisualizerScene() : Scene() {
        current_algorithm = Algorithm::QUICK_SORT;
        sort_phase = 0;
        delay_counter = 0;

        i = 0;
        j = 0;
        min_idx = 0;
        key = 0;
        bubble_pass_end = 0;
        cocktail_start = 0;
        cocktail_end = 0;
        cocktail_forward = true;
        shell_gap = 0;
        shell_i = 0;
        shell_j = 0;
        shell_key = 0;
        heap_end = 0;
        heap_build_root = 0;
        heap_sift_root = 0;
        heap_building = true;

        qs_low = 0;
        qs_high = 0;
        qs_pivot_value = 0;
        qs_pivot_index = -1;
        qs_i = 0;
        qs_j = 0;
        qs_partitioning = false;
        
        comb_gap = 0;
        comb_shrink = 1.3f;
        comb_swapped = false;

        gnome_pos = 1;

        ms_width = 1;
        ms_left = 0;
        ms_mid = 0;
        ms_right = 0;
        ms_i = 0;
        ms_j = 0;
        ms_k = 0;
        ms_merging = false;
    }

    void SortingVisualizerScene::reset_array() {
        int gap = std::max(0, bar_gap->get());
        int current_num_bars = (matrix_width + gap) / (1 + gap);
        if (current_num_bars < 1) {
            current_num_bars = 1;
        }

        array_data.clear();
        array_data.reserve(current_num_bars);
        for (int k = 0; k < current_num_bars; ++k) {
            array_data.push_back(k + 1);
        }

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(array_data.begin(), array_data.end(), g);

        sort_phase = 1;
        {
            std::lock_guard<std::mutex> lock(access_indices_mutex);
            access_indices.clear();
        }
        delay_counter = 0;
        last_step_time = std::chrono::steady_clock::now();

        int n = static_cast<int>(array_data.size());
        i = 0;
        j = 0;
        min_idx = 0;
        key = (n > 0) ? array_data[0] : 0;
        bubble_pass_end = std::max(0, n - 1);
        cocktail_start = 0;
        cocktail_end = std::max(0, n - 1);
        cocktail_forward = true;
        shell_gap = std::max(1, n / 2);
        shell_i = shell_gap;
        shell_j = shell_i - shell_gap;
        shell_key = (n > 0 && shell_i < n) ? array_data[shell_i] : 0;
        heap_end = std::max(0, n - 1);
        heap_build_root = std::max(0, (n - 2) / 2);
        heap_sift_root = 0;
        heap_building = n > 1;

        comb_gap = std::max(1, n);
        comb_shrink = 1.3f;
        comb_swapped = true;

        gnome_pos = 1;

        ms_width = 1;
        ms_left = 0;
        ms_mid = 0;
        ms_right = 0;
        ms_i = 0;
        ms_j = 0;
        ms_k = 0;
        ms_merging = false;
        ms_temp.clear();
        ms_temp.reserve(n);

        qs_stack.clear();
        if (n > 1) {
            qs_stack.push_back({0, n - 1});
        }
        qs_low = 0;
        qs_high = std::max(0, n - 1);
        qs_pivot_value = (n > 0) ? array_data.back() : 0;
        qs_pivot_index = -1;
        qs_i = 0;
        qs_j = 0;
        qs_partitioning = false;
    }

    void SortingVisualizerScene::pick_next_algorithm() {
        int next_algo = (static_cast<int>(current_algorithm) + 1) % static_cast<int>(Algorithm::MAX_ALGORITHM);
        current_algorithm = static_cast<Algorithm>(next_algo);
        reset_array();
    }

    bool SortingVisualizerScene::step_bubble() {
        int n = static_cast<int>(array_data.size());
        if (n < 2 || bubble_pass_end <= 0) {
            return true;
        }

        if (j < bubble_pass_end) {
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices = {j, j + 1};
            }
            if (array_data[j] > array_data[j + 1]) {
                std::swap(array_data[j], array_data[j + 1]);
            }
            ++j;
            return false;
        }

        j = 0;
        --bubble_pass_end;
        return bubble_pass_end <= 0;
    }

    bool SortingVisualizerScene::step_selection() {
        int n = static_cast<int>(array_data.size());
        if (n < 2 || i >= n - 1) {
            return true;
        }

        if (j < n) {
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices = {min_idx, j};
            }
            if (array_data[j] < array_data[min_idx]) {
                min_idx = j;
            }
            ++j;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(access_indices_mutex);
            access_indices = {i, min_idx};
        }
        std::swap(array_data[min_idx], array_data[i]);
        ++i;
        if (i >= n - 1) {
            return true;
        }

        min_idx = i;
        j = i + 1;
        return false;
    }

    bool SortingVisualizerScene::step_insertion() {
        int n = static_cast<int>(array_data.size());
        if (n < 2 || i >= n) {
            return true;
        }
        // Ensure proper initialization for insertion (start at index 1)
        if (i == 0) {
            i = 1;
            key = (n > 1) ? array_data[1] : array_data[0];
            j = i - 1;
        }

        {
            std::lock_guard<std::mutex> lock(access_indices_mutex);
            access_indices = {j, i};
        }
        if (j >= 0 && array_data[j] > key) {
            array_data[j + 1] = array_data[j];
            --j;
            return false;
        }

        array_data[j + 1] = key;
        ++i;
        if (i >= n) {
            return true;
        }

        key = array_data[i];
        j = i - 1;
        return false;
    }

    bool SortingVisualizerScene::step_cocktail() {
        int n = static_cast<int>(array_data.size());
        if (n < 2 || cocktail_start >= cocktail_end) {
            return true;
        }

        if (cocktail_forward) {
            if (j < cocktail_end) {
                {
                    std::lock_guard<std::mutex> lock(access_indices_mutex);
                    access_indices = {j, j + 1};
                }
                if (array_data[j] > array_data[j + 1]) {
                    std::swap(array_data[j], array_data[j + 1]);
                }
                ++j;
                return false;
            }

            if (cocktail_end > 0) {
                --cocktail_end;
            }
            cocktail_forward = false;
            j = cocktail_end;
            return false;
        }

        if (j > cocktail_start) {
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices = {j - 1, j};
            }
            if (array_data[j - 1] > array_data[j]) {
                std::swap(array_data[j - 1], array_data[j]);
            }
            --j;
            return false;
        }

        ++cocktail_start;
        cocktail_forward = true;
        j = cocktail_start;
        return cocktail_start >= cocktail_end;
    }

    bool SortingVisualizerScene::step_shell() {
        int n = static_cast<int>(array_data.size());
        if (n < 2) {
            return true;
        }

        if (shell_gap <= 0) {
            return true;
        }

        if (shell_i < n) {
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices = {shell_j, shell_i};
            }
            if (shell_j >= 0 && array_data[shell_j] > shell_key) {
                array_data[shell_j + shell_gap] = array_data[shell_j];
                shell_j -= shell_gap;
            } else {
                array_data[shell_j + shell_gap] = shell_key;
                ++shell_i;
                if (shell_i < n) {
                    shell_key = array_data[shell_i];
                    shell_j = shell_i - shell_gap;
                }
            }
            return false;
        }

        shell_gap /= 2;
        if (shell_gap <= 0) {
            return true;
        }

        shell_i = shell_gap;
        shell_j = shell_i - shell_gap;
        shell_key = array_data[shell_i];
        return false;
    }

    bool SortingVisualizerScene::step_comb() {
        int n = static_cast<int>(array_data.size());
        if (n < 2) return true;

        // Standard comb sort pass: reduce gap then run a full pass
        if (comb_gap > 1) {
            comb_gap = std::max(1, static_cast<int>(comb_gap / comb_shrink));
        }

        bool swapped = false;
        for (int idx = 0; idx + comb_gap < n; ++idx) {
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices = {idx, idx + comb_gap};
            }
            if (array_data[idx] > array_data[idx + comb_gap]) {
                std::swap(array_data[idx], array_data[idx + comb_gap]);
                swapped = true;
            }
        }

        comb_swapped = swapped;
        if (comb_gap <= 1 && !comb_swapped) {
            return true;
        }

        return false;
    }

    bool SortingVisualizerScene::step_gnome() {
        int n = static_cast<int>(array_data.size());
        if (n < 2) return true;
        if (gnome_pos >= n) return true;

        if (gnome_pos <= 0) {
            gnome_pos = 1;
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(access_indices_mutex);
            access_indices = {gnome_pos - 1, gnome_pos};
        }
        if (array_data[gnome_pos] >= array_data[gnome_pos - 1]) {
            ++gnome_pos;
        } else {
            std::swap(array_data[gnome_pos], array_data[gnome_pos - 1]);
            --gnome_pos;
        }

        return gnome_pos >= n;
    }

    bool SortingVisualizerScene::step_merge() {
        int n = static_cast<int>(array_data.size());
        if (n < 2) return true;

        if (!ms_merging) {
            if (ms_left >= n) {
                ms_left = 0;
                ms_width *= 2;
            }

            if (ms_width >= n) {
                return true;
            }

            ms_mid = std::min(ms_left + ms_width - 1, n - 1);
            ms_right = std::min(ms_left + 2 * ms_width - 1, n - 1);
            ms_i = ms_left;
            ms_j = ms_mid + 1;
            ms_temp.clear();
            ms_merging = true;
        }

        if (ms_merging) {
            while (ms_i <= ms_mid && ms_j <= ms_right) {
                {
                    std::lock_guard<std::mutex> lock(access_indices_mutex);
                    access_indices = {ms_i, ms_j};
                }
                if (array_data[ms_i] <= array_data[ms_j]) {
                    ms_temp.push_back(array_data[ms_i]);
                    ++ms_i;
                } else {
                    ms_temp.push_back(array_data[ms_j]);
                    ++ms_j;
                }
                // do one comparison per step
                return false;
            }

            while (ms_i <= ms_mid) {
                ms_temp.push_back(array_data[ms_i]);
                ++ms_i;
                return false;
            }

            while (ms_j <= ms_right) {
                ms_temp.push_back(array_data[ms_j]);
                ++ms_j;
                return false;
            }

            // copy back
            for (size_t k = 0; k < ms_temp.size(); ++k) {
                array_data[ms_left + static_cast<int>(k)] = ms_temp[k];
            }

            ms_merging = false;
            ms_left = ms_right + 1;
            return false;
        }

        return true;
    }

    bool SortingVisualizerScene::step_quicksort() {
        if (!qs_partitioning) {
            while (!qs_stack.empty()) {
                auto range = qs_stack.back();
                qs_stack.pop_back();
                qs_low = range.first;
                qs_high = range.second;

                if (qs_low < qs_high) {
                    qs_pivot_index = qs_high;
                    qs_pivot_value = array_data[qs_high];
                    qs_i = qs_low - 1;
                    qs_j = qs_low;
                    qs_partitioning = true;
                    break;
                }
            }

            if (!qs_partitioning) {
                return true;
            }
        }

        if (qs_j <= qs_high - 1) {
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices = {qs_j, qs_pivot_index};
            }
            if (array_data[qs_j] < qs_pivot_value) {
                ++qs_i;
                if (qs_i != qs_j) {
                    std::swap(array_data[qs_i], array_data[qs_j]);
                }
            }
            ++qs_j;
            return false;
        }

        int pivot_position = qs_i + 1;
        std::swap(array_data[pivot_position], array_data[qs_high]);
        qs_pivot_index = pivot_position;
        qs_partitioning = false;

        if (qs_low < pivot_position - 1) {
            qs_stack.push_back({qs_low, pivot_position - 1});
        }
        if (pivot_position + 1 < qs_high) {
            qs_stack.push_back({pivot_position + 1, qs_high});
        }

        return qs_stack.empty();
    }

    bool SortingVisualizerScene::step_heap() {
        int n = static_cast<int>(array_data.size());
        if (n < 2) {
            return true;
        }

        if (heap_building) {
            if (heap_build_root < 0) {
                heap_building = false;
                heap_end = n - 1;
                return false;
            }
            int left_child = heap_sift_root * 2 + 1;
            if (left_child > heap_end || heap_sift_root < 0) {
                heap_sift_root = heap_build_root;
            }

            int current_root = heap_sift_root;
            int largest = current_root;
            int child = current_root * 2 + 1;

            if (child <= heap_end) {
                largest = child;
                if (child + 1 <= heap_end && array_data[child + 1] > array_data[child]) {
                    largest = child + 1;
                }

                {
                    std::lock_guard<std::mutex> lock(access_indices_mutex);
                    access_indices = {current_root, largest};
                }
                if (array_data[largest] > array_data[current_root]) {
                    std::swap(array_data[current_root], array_data[largest]);
                    heap_sift_root = largest;
                    return false;
                }
            }

            --heap_build_root;
            heap_sift_root = heap_build_root;
            return false;
        }

        if (heap_end <= 0) {
            return true;
        }

        if (heap_sift_root == 0) {
            std::swap(array_data[0], array_data[heap_end]);
            --heap_end;
            heap_sift_root = 0;
            return false;
        }

        int current_root = heap_sift_root;
        int child = current_root * 2 + 1;
        if (child > heap_end) {
            heap_sift_root = 0;
            return heap_end <= 0;
        }

        int largest = child;
        if (child + 1 <= heap_end && array_data[child + 1] > array_data[child]) {
            largest = child + 1;
        }

        {
            std::lock_guard<std::mutex> lock(access_indices_mutex);
            access_indices = {current_root, largest};
        }
        if (array_data[largest] > array_data[current_root]) {
            std::swap(array_data[current_root], array_data[largest]);
            heap_sift_root = largest;
        } else {
            if (heap_end > 0) {
                std::swap(array_data[0], array_data[heap_end]);
                --heap_end;
            }
            heap_sift_root = 0;
        }

        return heap_end <= 0;
    }

    void SortingVisualizerScene::step_sort() {
        bool done = false;

        int speed_multiplier = 1;
        if (delay_ms->get() <= 0) {
            switch (current_algorithm) {
                case Algorithm::BUBBLE_SORT:
                case Algorithm::COCKTAIL_SORT:
                    speed_multiplier = 16;
                    break;
                case Algorithm::INSERTION_SORT:
                case Algorithm::SHELL_SORT:
                    speed_multiplier = 8;
                    break;
                case Algorithm::SELECTION_SORT:
                case Algorithm::QUICK_SORT:
                case Algorithm::HEAP_SORT:
                    speed_multiplier = 4;
                    break;
                case Algorithm::COMB_SORT:
                    speed_multiplier = 6;
                    break;
                case Algorithm::GNOME_SORT:
                    speed_multiplier = 8;
                    break;
                case Algorithm::MERGE_SORT:
                    speed_multiplier = 4;
                    break;
                case Algorithm::MAX_ALGORITHM:
                    break;
            }
        }

        for (int step = 0; step < speed_multiplier && !done; ++step) {
            switch (current_algorithm) {
                case Algorithm::BUBBLE_SORT:
                    done = step_bubble();
                    break;
                case Algorithm::INSERTION_SORT:
                    done = step_insertion();
                    break;
                case Algorithm::SELECTION_SORT:
                    done = step_selection();
                    break;
                case Algorithm::COCKTAIL_SORT:
                    done = step_cocktail();
                    break;
                case Algorithm::SHELL_SORT:
                    done = step_shell();
                    break;
                case Algorithm::QUICK_SORT:
                    done = step_quicksort();
                    break;
                case Algorithm::HEAP_SORT:
                    done = step_heap();
                    break;
                    case Algorithm::COMB_SORT:
                        done = step_comb();
                        break;
                    case Algorithm::GNOME_SORT:
                        done = step_gnome();
                        break;
                    case Algorithm::MERGE_SORT:
                        done = step_merge();
                        break;
                case Algorithm::MAX_ALGORITHM:
                    break;
            }
        }

        if (done) {
            sort_phase = 2;
            {
                std::lock_guard<std::mutex> lock(access_indices_mutex);
                access_indices.clear();
            }
        }
    }

    void SortingVisualizerScene::initialize(int width, int height) {
        Scene::initialize(width, height);
        matrix_width = matrix_width;
        matrix_height = matrix_height;
        reset_array();
    }

    bool SortingVisualizerScene::render(rgb_matrix::FrameCanvas *canvas) {
        canvas->Clear();

        int gap = std::max(0, bar_gap->get());
        int current_num_bars = (matrix_width + gap) / (1 + gap);
        if (current_num_bars < 1) {
            current_num_bars = 1;
        }

        if (static_cast<int>(array_data.size()) != current_num_bars) {
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
            ++delay_counter;
            if (delay_counter >= 100) {
                pick_next_algorithm();
            }
        }

        // Create a safe copy of access_indices to avoid race conditions during rendering
        std::vector<int> local_access_indices;
        {
            std::lock_guard<std::mutex> lock(access_indices_mutex);
            local_access_indices = access_indices;
        }

        int n = static_cast<int>(array_data.size());
        for (int x = 0; x < n; ++x) {
            int val = array_data[x];
            int bar_height = (val * matrix_height) / n;
            if (bar_height < 1) {
                bar_height = 1;
            }

            uint8_t r = 255;
            uint8_t g = 255;
            uint8_t b = 255;

            if (rainbow_mode->get()) {
                float hue = static_cast<float>(val) / static_cast<float>(n) * 360.0f;
                color::hsl_to_rgb(hue, 1.0f, 0.5f, r, g, b);
            } else {
                auto col = bar_color->get();
                r = col.r;
                g = col.g;
                b = col.b;
            }

            bool is_access = contains_index(local_access_indices, x);
            bool use_special_color = false;

            if (sort_phase == 2) {
                auto col = solved_color->get();
                r = col.r;
                g = col.g;
                b = col.b;
                use_special_color = true;
            } else {
                switch (current_algorithm) {
                    case Algorithm::BUBBLE_SORT:
                        if (x > bubble_pass_end) {
                            auto col = solved_color->get();
                            r = col.r;
                            g = col.g;
                            b = col.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::INSERTION_SORT:
                        if (x < i) {
                            auto col = solved_color->get();
                            r = col.r;
                            g = col.g;
                            b = col.b;
                            use_special_color = true;
                        } else if (x == i) {
                            r = kKeyColor.r;
                            g = kKeyColor.g;
                            b = kKeyColor.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::SELECTION_SORT:
                        if (x < i) {
                            auto col = solved_color->get();
                            r = col.r;
                            g = col.g;
                            b = col.b;
                            use_special_color = true;
                        } else if (x == min_idx) {
                            r = kPivotColor.r;
                            g = kPivotColor.g;
                            b = kPivotColor.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::COCKTAIL_SORT:
                        if (x < cocktail_start || x > cocktail_end) {
                            auto col = solved_color->get();
                            r = col.r;
                            g = col.g;
                            b = col.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::SHELL_SORT:
                        if (x < shell_i) {
                            auto col = solved_color->get();
                            r = col.r;
                            g = col.g;
                            b = col.b;
                            use_special_color = true;
                        } else if (x == shell_i) {
                            r = kKeyColor.r;
                            g = kKeyColor.g;
                            b = kKeyColor.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::QUICK_SORT:
                        if (qs_partitioning) {
                            if (x == qs_pivot_index) {
                                r = kPivotColor.r;
                                g = kPivotColor.g;
                                b = kPivotColor.b;
                                use_special_color = true;
                            } else if (x == qs_low) {
                                r = kPartitionLowColor.r;
                                g = kPartitionLowColor.g;
                                b = kPartitionLowColor.b;
                                use_special_color = true;
                            } else if (x == qs_high) {
                                r = kPartitionHighColor.r;
                                g = kPartitionHighColor.g;
                                b = kPartitionHighColor.b;
                                use_special_color = true;
                            } else if (x == qs_j) {
                                r = kAccessColor.r;
                                g = kAccessColor.g;
                                b = kAccessColor.b;
                                use_special_color = true;
                            } else if (x > qs_low && x < qs_high) {
                                if (x <= qs_i) {
                                    r = kPartitionLeftColor.r;
                                    g = kPartitionLeftColor.g;
                                    b = kPartitionLeftColor.b;
                                } else {
                                    r = kPartitionRightColor.r;
                                    g = kPartitionRightColor.g;
                                    b = kPartitionRightColor.b;
                                }
                                use_special_color = true;
                            }
                        }
                        // also highlight ranges that are still on the stack
                        for (const auto &range : qs_stack) {
                            if (x >= range.first && x <= range.second) {
                                r = static_cast<uint8_t>((r + 32) / 2);
                                g = static_cast<uint8_t>((g + 32) / 2);
                                b = static_cast<uint8_t>((b + 32) / 2);
                                use_special_color = true;
                                break;
                            }
                        }
                        break;
                    case Algorithm::COMB_SORT:
                        // highlight current gap comparisons and gaps
                        if (comb_gap > 1) {
                            if (x == j || x == j + comb_gap) {
                                r = kAccessColor.r;
                                g = kAccessColor.g;
                                b = kAccessColor.b;
                                use_special_color = true;
                            } else if ((x % comb_gap) == 0) {
                                r = kPartitionLeftColor.r;
                                g = kPartitionLeftColor.g;
                                b = kPartitionLeftColor.b;
                                use_special_color = true;
                            }
                        }
                        break;
                    case Algorithm::GNOME_SORT:
                        if (x == gnome_pos) {
                            r = kKeyColor.r;
                            g = kKeyColor.g;
                            b = kKeyColor.b;
                            use_special_color = true;
                        } else if (x == gnome_pos - 1) {
                            r = kAccessColor.r;
                            g = kAccessColor.g;
                            b = kAccessColor.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::MERGE_SORT:
                        if (ms_merging) {
                            if (x >= ms_left && x <= ms_right) {
                                if (x <= ms_mid) {
                                    r = kPartitionLeftColor.r;
                                    g = kPartitionLeftColor.g;
                                    b = kPartitionLeftColor.b;
                                } else {
                                    r = kPartitionRightColor.r;
                                    g = kPartitionRightColor.g;
                                    b = kPartitionRightColor.b;
                                }
                                use_special_color = true;
                            }
                        }
                        break;
                    case Algorithm::HEAP_SORT:
                        if (x > heap_end) {
                            auto col = solved_color->get();
                            r = col.r;
                            g = col.g;
                            b = col.b;
                            use_special_color = true;
                        } else if (heap_building && x == heap_build_root) {
                            r = kHeapBuildColor.r;
                            g = kHeapBuildColor.g;
                            b = kHeapBuildColor.b;
                            use_special_color = true;
                        } else if (!heap_building && x == heap_sift_root) {
                            r = kHeapRootColor.r;
                            g = kHeapRootColor.g;
                            b = kHeapRootColor.b;
                            use_special_color = true;
                        }
                        break;
                    case Algorithm::MAX_ALGORITHM:
                        break;
                }

                if (is_access && !use_special_color) {
                    r = kAccessColor.r;
                    g = kAccessColor.g;
                    b = kAccessColor.b;
                }
            }

            int screen_x = x * (1 + gap);
            if (screen_x >= matrix_width) {
                continue;
            }

            for (int y = matrix_height - bar_height; y < matrix_height; ++y) {
                canvas->SetPixel(screen_x, y, r, g, b);
            }

            canvas->SetPixel(screen_x, 0, r, g, b);
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
        add_property(solved_color);
    }

    std::unique_ptr<Scenes::Scene> SortingVisualizerSceneWrapper::create() {
        return std::make_unique<SortingVisualizerScene>();
    }
}