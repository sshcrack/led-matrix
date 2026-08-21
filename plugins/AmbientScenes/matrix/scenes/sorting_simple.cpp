#include "SortingVisualizerScene.h"
#include <algorithm>

namespace AmbientScenes {

bool SortingVisualizerScene::step_bubble() {
    int n = static_cast<int>(array_data.size());
    if (n < 2 || bubble_pass_end <= 0) {
        return true;
    }

    if (j < bubble_pass_end) {
        access_indices = {j, j + 1};
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
        access_indices = {min_idx, j};
        if (array_data[j] < array_data[min_idx]) {
            min_idx = j;
        }
        ++j;
        return false;
    }

    access_indices = {i, min_idx};
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

    if (i == 0) {
        i = 1;
        key = (n > 1) ? array_data[1] : array_data[0];
        j = i - 1;
    }

    access_indices = {j, i};
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
            access_indices = {j, j + 1};
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
        access_indices = {j - 1, j};
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

bool SortingVisualizerScene::step_gnome() {
    int n = static_cast<int>(array_data.size());
    if (n < 2) return true;
    if (gnome_pos >= n) return true;

    if (gnome_pos <= 0) {
        gnome_pos = 1;
        return false;
    }

    access_indices = {gnome_pos - 1, gnome_pos};
    if (array_data[gnome_pos] >= array_data[gnome_pos - 1]) {
        ++gnome_pos;
    } else {
        std::swap(array_data[gnome_pos], array_data[gnome_pos - 1]);
        --gnome_pos;
    }

    return gnome_pos >= n;
}

}
