#include "SortingVisualizerScene.h"
#include <algorithm>

namespace AmbientScenes {

bool SortingVisualizerScene::step_shell() {
    int n = static_cast<int>(array_data.size());
    if (n < 2) {
        return true;
    }

    if (shell_gap <= 0) {
        return true;
    }

    if (shell_i < n) {
        access_indices = {shell_j, shell_i};
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
        access_indices = {qs_j, qs_pivot_index};
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

            access_indices = {current_root, largest};
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
        // Fall through to sift down the new root
    }

    if (heap_sift_root < 0) {
        heap_sift_root = 0;
        return heap_end <= 0;
    }
    int current_root = heap_sift_root;
    int child = current_root * 2 + 1;

    if (child <= heap_end) {
        int largest = child;
        if (child + 1 <= heap_end && array_data[child + 1] > array_data[child]) {
            largest = child + 1;
        }

        access_indices = {current_root, largest};
        if (array_data[largest] > array_data[current_root]) {
            std::swap(array_data[current_root], array_data[largest]);
            heap_sift_root = largest;
            return false;
        }
    }

    heap_sift_root = 0;
    return heap_end <= 0;
}

bool SortingVisualizerScene::step_comb() {
    int n = static_cast<int>(array_data.size());
    if (n < 2) return true;

    if (comb_gap > 1) {
        comb_gap = std::max(1, static_cast<int>(comb_gap / comb_shrink));
    }

    if (comb_idx < 0) comb_idx = 0;

    if (comb_idx + comb_gap < n) {
        access_indices = {comb_idx, comb_idx + comb_gap};
        if (array_data[comb_idx] > array_data[comb_idx + comb_gap]) {
            std::swap(array_data[comb_idx], array_data[comb_idx + comb_gap]);
            comb_swapped = true;
        }
        ++comb_idx;
        return false;
    }

    comb_idx = 0;
    if (comb_gap <= 1 && !comb_swapped) {
        return true;
    }

    comb_swapped = false;
    return false;
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
            access_indices = {ms_i, ms_j};
            if (array_data[ms_i] <= array_data[ms_j]) {
                ms_temp.push_back(array_data[ms_i]);
                ++ms_i;
            } else {
                ms_temp.push_back(array_data[ms_j]);
                ++ms_j;
            }
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

        for (size_t k = 0; k < ms_temp.size(); ++k) {
            array_data[ms_left + static_cast<int>(k)] = ms_temp[k];
        }

        ms_merging = false;
        ms_left = ms_right + 1;
        return false;
    }

    return true;
}

}
