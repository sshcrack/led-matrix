#include "canvas.h"
#include "shared/utils/utils.h"
#include "shared/utils/shared.h"
#include "shared/interrupt.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

using rgb_matrix::RGBMatrix;


FrameCanvas* update_canvas(RGBMatrix *matrix, FrameCanvas* offscreen_canvas) {
    auto preset = config->get_curr();
    auto scenes = preset.scenes;

    for (const auto &item: scenes) {
        if (!item->is_initialized())
            item->initialize(matrix, offscreen_canvas);
    }


    while (!exit_canvas_update) {
        int total_weight = 0;

        vector<std::pair<int, std::shared_ptr<Scenes::Scene>>> weighted_scenes;
        for (const auto &item: scenes) {
            auto weight = item->get_weight();

            weighted_scenes.emplace_back(weight, item);
            total_weight += weight;
        }

        auto selected = get_random_number_inclusive(0, total_weight);
        int curr_weight = 0;

        std::shared_ptr<Scenes::Scene> scene;
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


        info("Displaying scene {}", scene->get_name());
        tmillis_t start_ms = GetTimeInMillis();
        tmillis_t end_ms = start_ms + scene->get_duration();

        scene->offscreen_canvas = offscreen_canvas;
        while (GetTimeInMillis() < end_ms) {
            auto should_continue = scene->render(matrix);

            if (!should_continue || interrupt_received || exit_canvas_update) {
                debug("Exiting scene early.");
                break;
            }

            //SleepMillis(10);
        }

        scene->after_render_stop(matrix);
        offscreen_canvas = scene->offscreen_canvas;
    }

    return offscreen_canvas;
}
