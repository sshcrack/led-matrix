#include "canvas.h"
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

#include <shared/matrix/server/common.h>

#include "shared/matrix/utils/utils.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/plugin_loader/loader.h"
#include <spdlog/spdlog.h>

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

using namespace std;
using namespace spdlog;

using rgb_matrix::RGBMatrixBase;


FrameCanvas *update_canvas(RGBMatrixBase *matrix, FrameCanvas *pCanvas) {
    const auto preset = config->get_curr();
    auto scenes = preset->scenes;

    for (const auto &item: scenes) {
        if (!item->is_initialized())
            item->initialize(matrix, pCanvas);
    }

    int cantFindScene = 0;
    while (!exit_canvas_update) {
        int total_weight = 0;

        vector<std::pair<int, std::shared_ptr<Scenes::Scene> > > weighted_scenes;
        for (const auto &item: scenes) {
            auto weight = item->get_weight();
            if(weight <= 0)
                continue;

            weighted_scenes.emplace_back(weight, item);
            total_weight += weight;
        }

        const auto selected = get_random_number_inclusive(0, total_weight);
        int curr_weight = 0;

        std::shared_ptr<Scenes::Scene> scene;
        for (const auto &[weight, curr_scene]: weighted_scenes) {
            curr_weight += weight;

            if (curr_weight >= selected) {
                scene = curr_scene;
                break;
            }
        }

        if (scene == nullptr) {
            if (cantFindScene < 3)
                error("Could not find scene to display.");
            cantFindScene++;

            SleepMillis(300);
            Server::currScene = nullptr;
            continue;
        }

        cantFindScene = 0;
        const tmillis_t start_ms = GetTimeInMillis();
        const tmillis_t end_ms = start_ms + scene->get_duration();
        spdlog::debug("Curr scene mutex");
        {
            std::unique_lock lock(Server::currSceneMutex);
            Server::currScene = scene;
        }

        spdlog::debug("Registry mutex");
        {
            std::shared_lock lock(Server::registryMutex);
            spdlog::debug("Now displaying scene: {}", scene->get_name());
            for (const auto ws_handle: Server::registry | views::values) {
                restinio::websocket::basic::message_t message;
                message.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
                message.set_payload("active:" + scene->get_name());

                ws_handle->send_message(message);
            }
        }

        if(scene->offscreen_canvas != nullptr)
            scene->offscreen_canvas = pCanvas;

        Constants::isRenderingSceneInitially = true;
        while (GetTimeInMillis() < end_ms) {
            const auto should_continue = scene->render(matrix);
            Constants::isRenderingSceneInitially = false;


            if (!should_continue || interrupt_received || exit_canvas_update) {
                trace("Exiting scene early.");
                break;
            }

            // Check for beat detection from any plugin and trigger post-processing
            if (Constants::global_post_processor && scene->offscreen_canvas != nullptr) {
                Constants::global_post_processor->process_canvas(matrix, scene->offscreen_canvas);
            }

            if(scene->offscreen_canvas != nullptr && should_continue) {
                scene->offscreen_canvas = matrix->SwapOnVSync(scene->offscreen_canvas, 1);
            }

#ifdef ENABLE_EMULATOR
            ((rgb_matrix::EmulatorMatrix *)matrix)->Render();
        #endif
        }

        spdlog::debug("Exiting scene: {}", scene->get_name());
        scene->after_render_stop(matrix);
        if(scene->offscreen_canvas != nullptr)
            pCanvas = scene->offscreen_canvas;
    }

    return pCanvas;
}
