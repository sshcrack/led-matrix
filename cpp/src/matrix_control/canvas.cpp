#include "canvas.h"
#include "shared/utils/utils.h"
#include "shared/utils/shared.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

using rgb_matrix::RGBMatrix;


void update_canvas(RGBMatrix *matrix) {
    auto preset = config->get_curr();
    auto scenes = preset.scenes;

    for (const auto &item: scenes) {
        if (item->is_initialized())
            item->initialize(matrix);
    }


    while (!exit_canvas_update) {
        int total_weight = 0;

        vector<std::pair<int, Scenes::Scene *>> weighted_scenes;
        for (const auto &item: scenes) {
            auto weight = item->get_weight();

            weighted_scenes.emplace_back(weight, item);
            total_weight += weight;
        }

        auto selected = get_random_number_inclusive(0, total_weight);
        int curr_weight = 0;

        Scenes::Scene *scene = nullptr;
        for (const auto &item: weighted_scenes) {
            curr_weight += item.first;

            if (curr_weight >= selected) {
                scene = item.second;
                break;
            }
        }

        if (scene == nullptr) {
            error("Could not find scene to display.");
            SleepMillis(300);
            continue;
        }


        tmillis_t start_ms = GetTimeInMillis();
        tmillis_t end_ms = start_ms + scene->get_duration();

        while (GetTimeInMillis() < end_ms) {
            scene->tick(matrix);
            SleepMillis(10);
        }
    }
}
