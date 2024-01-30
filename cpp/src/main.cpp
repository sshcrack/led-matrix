// scraper.cpp

#include <iostream>

#include "led-matrix.h"
#include "graphics.h"
#include "canvas.h"
#include "pixel_art.h"
#include "image.h"
#include "interrupt.h"

#include <signal.h>
#include <Magick++.h>

using namespace rgb_matrix;

int usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

int main(int argc, char *argv[])
{
    Magick::InitializeMagick(*argv);

    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                           &matrix_options, &runtime_opt))
    {
        return usage(argv[0]);
    }

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == nullptr)
        return usage(argv[0]);

    rgb_matrix::FrameCanvas *canvas = matrix->CreateFrameCanvas();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    std::cout << "Getting page size..." << std::endl;
    std::optional<int> page_end_opt = get_page_size();
    if(!page_end_opt.has_value()) {
        std::cerr << "could not convert page " << std::endl;
        return -1;
    }

    int page_end = page_end_opt.value();

    std::cout << "Press Q to quit" << std::endl;
    while (!interrupt_received)
    {
        update_canvas(canvas, matrix, page_end);
        canvas = matrix->SwapOnVSync(canvas);
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    printf("\n");

    return 0;
}