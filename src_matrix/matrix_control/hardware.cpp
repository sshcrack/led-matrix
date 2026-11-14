#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <restinio/websocket/websocket.hpp>

#include "LiveFrameSnapshot.h"
#include "MatrixPresenter.h"
#include "canvas.h"
#include "led-matrix.h"
#include "server/desktop_ws.h"
#include "shared/common/timesource/TimeSource.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/server/common.h"
#include "shared/matrix/server/server_utils.h"
#include "shared/matrix/utils/shared.h"  // PlaybackControl
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

void hardware_mainloop(rgb_matrix::RGBMatrixBase* matrix, PostProcessor* post_processor, TransitionManager* transition_manager,
                       Config::MainConfig* cfg, std::shared_ptr<Scenes::Scene> pinned_scene)
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

    CanvasCoordinator coordinator(matrix, &wall_clock, post_processor, transition_manager, presenter.get(), cfg,
                                  &PlaybackControl::restart_flag(), &interrupt_received, RuntimeInputs::snapshot, set_curr_scene,
                                  broadcast);

    string last_scheduled_preset = "";

    while (!interrupt_received)
    {
        if (cfg->is_scheduling_enabled()) {
            auto active_preset = cfg->get_active_scheduled_preset();
            if (active_preset.has_value() && active_preset.value() != last_scheduled_preset)
            {
                debug("Switching to scheduled preset: {}", active_preset.value());
                cfg->set_curr(active_preset.value());
                last_scheduled_preset = active_preset.value();
                cfg->set_turned_off(false);
                Server::broadcast_matrix_enabled(true);
            }
            else if (!active_preset.has_value() && !last_scheduled_preset.empty())
            {

                debug("No active schedule, clearing scheduled preset and turning off canvas");
                last_scheduled_preset = "";
                cfg->set_turned_off(true);
                Server::broadcast_matrix_enabled(false);
            }
        }

        if (!cfg->is_turned_off()) {
            // Consume pending restart requests before run() samples config and
            // playback state: a request that arrives while run() is running (or
            // returning) stays set and ends the next run() instead of being lost.
            PlaybackControl::acknowledge();
            coordinator.run(pinned_scene);
            debug("Outer loop iteration, checking again...");
            continue;
        }

        matrix->Clear();
        LiveFrame::SnapshotStore::instance().publish_solid_if_requested(
            matrix->width(), matrix->height(), 0, 0, 0);
        SleepMillis(1000);
    }

    if (const int signo = interrupt_signal.load())
        info("Interrupt signal received: {}", signo);
    info("Finished, shutting down...");
}

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase* matrix, PostProcessor* post_processor, TransitionManager* transition_manager,
                            Config::MainConfig* cfg, std::shared_ptr<Scenes::Scene> pinned_scene)
{

    Constants::height = matrix->height();
    Constants::width = matrix->width();

    struct sigaction action{};
    action.sa_handler = InterruptHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);

    debug("Running hardware mainloop...");
    hardware_mainloop(matrix, post_processor, transition_manager, cfg, pinned_scene);
    return 0;
}
