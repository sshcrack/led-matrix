#include <iostream>

#include "led-matrix.h"
#include "canvas.h"
#include "pixel_art.h"
#include "image.h"
#include "../interrupt.h"
#include "../shared.h"

#include <csignal>
#include <vector>
#include <expected>
#include <variant>
#include <Magick++.h>
#include <numeric>
#include <random>
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

void hardware_mainloop(int page_end, RGBMatrix *matrix, FrameCanvas *canvas) {
    while(!interrupt_received) {
        vector<int> total_pages(page_end);
        iota(total_pages.begin(), total_pages.end(), 1);

        shuffle(total_pages.begin(), total_pages.end(), random_device());

        cout << "Press Ctrl+C to quit" << endl;
        while (!config->is_dirty() && !interrupt_received)
        {
            update_canvas(canvas, matrix, &total_pages);
        }

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

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    std::optional<int> page_end_opt = get_page_size();
    if(!page_end_opt.has_value()) {
        error("could not convert page");
        return unexpected(-1);
    }

    int page_end = page_end_opt.value();

    return std::async(hardware_mainloop, page_end, matrix, canvas);
}