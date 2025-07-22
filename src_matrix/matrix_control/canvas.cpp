#include "canvas.h"
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

#include <server/common.h>

#include "shared/matrix/utils/utils.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/interrupt.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

using rgb_matrix::RGBMatrixBase;

FrameCanvas *update_canvas(RGBMatrixBase *matrix, FrameCanvas *pCanvas)
{
    const auto preset = config->get_curr();
    auto scenes = preset->scenes;

    for (const auto &item : scenes)
    {
        if (!item->is_initialized())
            item->initialize(matrix, pCanvas);
    }

    int cantFindScene = 0;
    while (!exit_canvas_update)
    {
        int total_weight = 0;

        vector<std::pair<int, std::shared_ptr<Scenes::Scene>>> weighted_scenes;
        for (const auto &item : scenes)
        {
            auto weight = item->get_weight();

            weighted_scenes.emplace_back(weight, item);
            total_weight += weight;
        }

        const auto selected = get_random_number_inclusive(0, total_weight);
        int curr_weight = 0;

        std::shared_ptr<Scenes::Scene> scene;
        for (const auto &[weight, curr_scene] : weighted_scenes)
        {
            curr_weight += weight;

            if (curr_weight >= selected)
            {
                scene = curr_scene;
                break;
            }
        }

        if (scene == nullptr)
        {
            if (cantFindScene < 3)
                error("Could not find scene to display.");
            cantFindScene++;

            SleepMillis(300);
            continue;
        }

        cantFindScene = 0;
        const tmillis_t start_ms = GetTimeInMillis();
        const tmillis_t end_ms = start_ms + scene->get_duration();

        {
            std::unique_lock lock(currSceneMutex);
            currScene = scene->get_name();
        }

        {
            std::unique_lock lock(registryMutex);
            for (const auto ws_handle : registry | views::values)
            {
                restinio::websocket::basic::message_t message;
                message.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
                message.set_payload(scene->get_name());

                ws_handle->send_message(message);
            }
        }

        scene->offscreen_canvas = pCanvas;
        while (GetTimeInMillis() < end_ms)
        {
            const auto should_continue = scene->render(matrix);

            if (!should_continue || interrupt_received || exit_canvas_update)
            {
                // I removed this log, this seems to spam if there is no scene to display
                // debug("Exiting scene early.");
                break;
            }

            // SleepMillis(10);
        }

        scene->after_render_stop(matrix);
        pCanvas = scene->offscreen_canvas;
    }

    return pCanvas;
}
