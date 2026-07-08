#include "canvas.h"

#include <restinio/core.hpp>
#include <restinio/websocket/websocket.hpp>

#include <shared/matrix/server/common.h>
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "spdlog/spdlog.h"

#ifdef ENABLE_EMULATOR
#include "emulator.h"
#endif

using namespace std;
using namespace spdlog;

CanvasCoordinator::CanvasCoordinator(RGBMatrixBase *matrix)
    : matrix_(matrix)
    , renderer_(matrix)
    , transition_engine_(matrix)
{
    first_offscreen_canvas_ = matrix->CreateFrameCanvas();
    second_offscreen_canvas_ = matrix->CreateFrameCanvas();
    composite_offscreen_canvas_ = matrix->CreateFrameCanvas();
}

CanvasCoordinator::~CanvasCoordinator() = default;

void CanvasCoordinator::run(std::shared_ptr<Scenes::Scene> pinned_scene)
{
    std::shared_ptr<ConfigData::Preset> preset = config->get_curr();
    if (!preset) {
        error("config->get_curr() returned null, using fallback preset");
        preset = ConfigData::Preset::create_default();
    }

    const auto &scenes = preset->scenes;
    const int matrix_width = matrix_->width();
    const int matrix_height = matrix_->height();

    for (const auto &item : scenes) {
        if (!item->is_initialized())
            item->initialize(matrix_width, matrix_height);
    }

    auto &first = first_offscreen_canvas_;
    auto &second = second_offscreen_canvas_;
    auto &composite = composite_offscreen_canvas_;

    int no_scene_count = 0;
    while (!exit_canvas_update) {
        bool is_desktop_connected = Server::is_desktop_connected();

        std::shared_ptr<Scenes::Scene> scene =
            pinned_scene ? pinned_scene : forced_scene_;
        std::string exclude_name = scene ? scene->get_name() : "";
        forced_scene_ = nullptr;

        if (scene == nullptr) {
            auto weighted = scheduler_.build_weighted_scenes(scenes, is_desktop_connected, exclude_name);
            scene = scheduler_.select_scene(weighted);
        }

        if (scene == nullptr) {
            if (no_scene_count < 3)
                error("Could not find scene to display.");
            no_scene_count++;

            Server::currScene = nullptr;
            renderer_.render_fallback();

#ifdef ENABLE_EMULATOR
            static_cast<rgb_matrix::EmulatorMatrix *>(matrix_)->Render();
#endif

            SleepMillis(300);
            continue;
        }

        no_scene_count = 0;
        const tmillis_t end_ms = GetTimeInMillis() + scene->get_duration();

        {
            unique_lock lock(Server::currSceneMutex);
            Server::currScene = scene;
        }

        {
            shared_lock lock(Server::registryMutex);
            debug("Now displaying scene: {}", scene->get_name());
            for (const auto ws_handle : Server::registry | views::values) {
                restinio::websocket::basic::message_t msg;
                msg.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
                msg.set_payload("active:" + scene->get_name());
                ws_handle->send_message(msg);
            }
        }

        std::shared_ptr<Scenes::Scene> next_scene;
        const auto transition_duration =
            scheduler_.resolve_transition_duration(preset, scene);
        const auto transition_name =
            scheduler_.resolve_transition_name(preset, scene);
        if (scheduler_.should_schedule_transition(transition_duration, scene->get_duration())
            && !pinned_scene) {
            auto weighted = scheduler_.build_weighted_scenes(scenes, is_desktop_connected,
                scene != nullptr ? scene->get_name() : "");
            next_scene = scheduler_.select_scene(weighted);
            if (next_scene != nullptr && !next_scene->is_initialized())
                next_scene->initialize(matrix_width, matrix_height);
        }

        bool early_exit = renderer_.render_scene_phase(scene, composite, end_ms);

        if (!early_exit && next_scene != nullptr) {
            transition_engine_.render_transition_phase(
                scene, next_scene,
                first, second, composite,
                matrix_width, matrix_height,
                transition_duration, transition_name, forced_scene_);
        }

        scene->after_render_stop();
    }
}
