#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "shared/interrupt.h"
#include "shared/utils/shared.h"

#include <csignal>
#include <expected>
#include "spdlog/spdlog.h"

using namespace rgb_matrix;
using namespace spdlog;

int usage(const char *progname) {
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

void hardware_mainloop(rgb_matrix::RGBMatrix *matrix) {
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

int start_hardware_mainloop(int argc, char *argv[]) {
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;

    debug("Parsing rgb matrix from cmdline");
    runtime_opt.drop_priv_user = getenv("SUDO_UID");
    runtime_opt.drop_priv_group = getenv("SUDO_GID");

    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                           &matrix_options, &runtime_opt)) {
        return usage(argv[0]);
    }

    rgb_matrix::RGBMatrix *matrix = rgb_matrix::RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == nullptr)
        return usage(argv[0]);

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    debug("Running hardware mainloop...");
    hardware_mainloop(matrix);
    return 0;
}