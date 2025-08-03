#include "canvas.h"
#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

#include <shared/matrix/server/common.h>

#include "shared/matrix/utils/utils.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/interrupt.h"
#include <spdlog/spdlog.h>

using namespace std;
using namespace spdlog;

using rgb_matrix::RGBMatrixBase;

// Global post-processor instance
PostProcessor* global_post_processor = nullptr;

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
        {
            std::unique_lock lock(Server::currSceneMutex);
            Server::currScene = scene;
        }

        {
            std::shared_lock lock(Server::registryMutex);
            spdlog::info("Updating scene: {}", scene->get_name());
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

            if(scene->offscreen_canvas != nullptr && should_continue) {
                scene->offscreen_canvas = matrix->SwapOnVSync(scene->offscreen_canvas, 1);
            }

            if (!should_continue || interrupt_received || exit_canvas_update) {
                // I removed this log, this seems to spam if there is no scene to display
                // debug("Exiting scene early.");
                break;
            }
<<<<<<< HEAD
=======

            // Check for beat detection from any plugin and trigger post-processing
            if (global_post_processor) {
                auto plugins = Plugins::PluginManager::instance()->get_plugins();
                for (auto &plugin : plugins) {
                    if (plugin->is_beat_detected()) {
                        // Add flash effect for beats (user can configure this later)
                        global_post_processor->add_effect(PostProcessType::Flash, 0.4f, 0.8f);
                        plugin->clear_beat_flag();
                        
                        // Send WebSocket notification for beat detection
                        {
                            std::shared_lock lock(Server::registryMutex);
                            for (const auto ws_handle: Server::registry | std::views::values) {
                                restinio::websocket::basic::message_t message;
                                message.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
                                message.set_payload("beat_detected");
                                ws_handle->send_message(message);
                            }
                        }
                        
                        break; // Only process one beat per frame
                    }
                }
                
                // Apply post-processing effects to the canvas
                global_post_processor->process_canvas(matrix, scene->offscreen_canvas);
            }

            // SleepMillis(10);
>>>>>>> origin/copilot/fix-e8beabcc-bb3f-4da7-9a64-afd1bca7016e
        }

        scene->after_render_stop(matrix);
        if(scene->offscreen_canvas != nullptr)
            pCanvas = scene->offscreen_canvas;
    }

    return pCanvas;
}
