#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "shared/matrix/interrupt.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/canvas_consts.h"

#include <csignal>
#include <chrono>
#include "spdlog/spdlog.h"

using namespace rgb_matrix;
using namespace spdlog;

void hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix)
{
    info("Press Ctrl+C to quit");

    FrameCanvas *first_offscreen_canvas = matrix->CreateFrameCanvas();
    FrameCanvas *second_offscreen_canvas = matrix->CreateFrameCanvas();
    FrameCanvas *composite_offscreen_Canvas = matrix->CreateFrameCanvas();
    string last_scheduled_preset = "";
    std::shared_ptr<Scenes::Scene> forced_scene = nullptr;

    while (!interrupt_received)
    {
        // Check for active scheduled preset
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
            update_canvas(matrix, first_offscreen_canvas, second_offscreen_canvas, composite_offscreen_Canvas, forced_scene);
            exit_canvas_update = false;
            debug("Outer loop iteration, checking again...");
            continue;
        }

        matrix->Clear();
        SleepMillis(1000);
    }

    // Cleanup post-processor
    if (Constants::global_post_processor)
    {
        delete Constants::global_post_processor;
        Constants::global_post_processor = nullptr;
        spdlog::info("Post-processor cleaned up");
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    info("Finished, shutting down...");
}

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix)
{

    Constants::height = matrix->height();
    Constants::width = matrix->width();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    debug("Running hardware mainloop...");
    hardware_mainloop(matrix);
    return 0;
}