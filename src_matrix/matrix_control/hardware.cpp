#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "MatrixPresenter.h"
#include "LiveFrameSnapshot.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/common.h"
#include "shared/matrix/server/server_utils.h"

#include <csignal>
#include <functional>
#include <restinio/websocket/websocket.hpp>
#include "spdlog/spdlog.h"

using namespace rgb_matrix;
using namespace spdlog;

static std::unique_ptr<MatrixPresenter> create_presenter(RGBMatrixBase *matrix)
{
    (void)matrix;
#ifdef ENABLE_EMULATOR
    return std::make_unique<EmulatorPresenter>(matrix);
#else
    return std::make_unique<NoopPresenter>(matrix);
#endif
}

void hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix, std::shared_ptr<Scenes::Scene> pinned_scene)
{
    info("Press Ctrl+C to quit");

    WallClock wall_clock;
    auto presenter = create_presenter(matrix);

    auto set_curr_scene = [](std::shared_ptr<Scenes::Scene> scene) {
        std::unique_lock lock(Server::currSceneMutex);
        Server::currScene = std::move(scene);
    };

    auto broadcast = [](const std::string &name) {
        std::shared_lock lock(Server::registryMutex);
        for (const auto &entry : Server::registry) {
            restinio::websocket::basic::message_t msg;
            msg.set_opcode(restinio::websocket::basic::opcode_t::text_frame);
            msg.set_payload("active:" + name);
            entry.second->send_message(msg);
        }
    };

    CanvasCoordinator coordinator(matrix, &wall_clock,
        Constants::global_post_processor,
        Constants::global_transition_manager,
        presenter.get(), config,
        &exit_canvas_update, &interrupt_received,
        Server::is_desktop_connected,
        set_curr_scene, broadcast);

    string last_scheduled_preset = "";

    while (!interrupt_received)
    {
        if (config->is_scheduling_enabled())
        {
            auto active_preset = config->get_active_scheduled_preset();
            if (active_preset.has_value() && active_preset.value() != last_scheduled_preset)
            {
                debug("Switching to scheduled preset: {}", active_preset.value());
                config->set_curr(active_preset.value());
                last_scheduled_preset = active_preset.value();
                config->set_turned_off(false);
            }
            else if (!active_preset.has_value() && !last_scheduled_preset.empty())
            {

                debug("No active schedule, clearing scheduled preset and turning off canvas");
                last_scheduled_preset = "";
                config->set_turned_off(true);
            }
        }

        if (!config->is_turned_off())
        {
            coordinator.run(pinned_scene);
            exit_canvas_update = false;
            debug("Outer loop iteration, checking again...");
            continue;
        }

        matrix->Clear();
        LiveFrame::SnapshotStore::instance().publish_solid_if_requested(
            matrix->width(), matrix->height(), 0, 0, 0);
        SleepMillis(1000);
    }

    info("Finished, shutting down...");
}

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix, std::shared_ptr<Scenes::Scene> pinned_scene)
{

    Constants::height = matrix->height();
    Constants::width = matrix->width();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    debug("Running hardware mainloop...");
    hardware_mainloop(matrix, pinned_scene);
    return 0;
}
