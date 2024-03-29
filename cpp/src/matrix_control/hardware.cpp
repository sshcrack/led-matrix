#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "pixel_art.h"
#include "../interrupt.h"
#include "../utils/consts.h"
#include "../utils/shared.h"

#include <csignal>
#include <expected>
#include <variant>
#include <future>
#include "spdlog/spdlog.h"

using namespace rgb_matrix;
using namespace spdlog;

int usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

void hardware_mainloop(RGBMatrix *matrix, FrameCanvas *canvas) {
    cout << "Press Ctrl+C to quit" << endl;
    while(!interrupt_received) {
        exit_canvas_update = false;
        update_canvas(canvas, matrix);
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    info("Finished, shutting down...");
}

expected<std::future<void>, int> initialize_hardware(int argc, char *argv[]) {
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;

    debug("Parsing rgb matrix from cmdline");
    runtime_opt.drop_priv_user = getenv("SUDO_UID");
    runtime_opt.drop_priv_group = getenv("SUDO_GID");

    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                           &matrix_options, &runtime_opt))
    {
        return unexpected(usage(argv[0]));
    }

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == nullptr)
        return unexpected(usage(argv[0]));

    rgb_matrix::FrameCanvas *canvas = matrix->CreateFrameCanvas();
    Constants::height = canvas->height();
    Constants::width = canvas->width();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    return std::async(hardware_mainloop, matrix, canvas);
}