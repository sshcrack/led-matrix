#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "shared/interrupt.h"
#include "shared/utils/shared.h"
#include "utils/canvas_consts.h"

#include <csignal>
#include "spdlog/spdlog.h"

using namespace rgb_matrix;
using namespace spdlog;

void hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix) {
    info("Press Ctrl+C to quit");

    FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
    while (!interrupt_received) {
        offscreen_canvas = update_canvas(matrix, offscreen_canvas);
        exit_canvas_update = false;

        while (turned_off) {
            matrix->Clear();
            SleepMillis(1000);
        }
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    info("Finished, shutting down...");
}

int start_hardware_mainloop(rgb_matrix::RGBMatrixBase *matrix) {

    Constants::height = matrix->height();
    Constants::width = matrix->width();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    debug("Running hardware mainloop...");
    hardware_mainloop(matrix);
    return 0;
}