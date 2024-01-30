// scraper.cpp

#include <iostream>

#include "led-matrix.h"
#include "graphics.h"
#include "canvas.h"
#include "pixel_art.h"
#include "image.h"
#include "interrupt.h"

#include <csignal>
#include <Magick++.h>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"

using namespace rgb_matrix;
using namespace spdlog;

int usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

int main(int argc, char *argv[])
{
    Magick::InitializeMagick(*argv);
    spdlog::cfg::load_env_levels();

    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;

    debug("Parsing rgb matrix from cmdline");
    runtime_opt.drop_priv_user = getenv("SUDO_UID");
    runtime_opt.drop_priv_group = getenv("SUDO_GID");
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                           &matrix_options, &runtime_opt))
    {
        return usage(argv[0]);
    }

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == nullptr)
        return usage(argv[0]);

    debug("Creating canvas");
    rgb_matrix::FrameCanvas *canvas = matrix->CreateFrameCanvas();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    std::optional<int> page_end_opt = get_page_size();
    if(!page_end_opt.has_value()) {
        error("could not convert page");
        return -1;
    }

    int page_end = page_end_opt.value();

    std::cout << "Press Q to quit" << std::endl;
    while (!interrupt_received)
    {
        update_canvas(canvas, matrix, page_end);
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    printf("\n");

    return 0;
}