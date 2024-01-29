// scraper.cpp

#include <iostream>
#include "pixel_art.h"

#include "led-matrix.h"
#include "graphics.h"
#include "canvas.h"

#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <deque>

#include "interrupt.h"

using namespace rgb_matrix;

int usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

int main(int argc, char *argv[])
{
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                           &matrix_options, &runtime_opt))
    {
        return usage(argv[0]);
    }

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
        return usage(argv[0]);

    rgb_matrix::FrameCanvas *canvas = matrix->CreateFrameCanvas();

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);
    bool running = true;
    std::cout << "Press Q to quit" << std::flush;
    while (!interrupt_received && running)
    {
        update_canvas(canvas);
        canvas = matrix->SwapOnVSync(canvas);

        const char c = tolower(getch());
        switch (c)
        {
            // All kinds of conditions which we use to exit
        case 0x1B: // Escape
        case 'q':  // 'Q'uit
        case 0x04: // End of file
        case 0x00: // Other issue from getch()
            running = false;
            break;
        }
    }

    // Finished. Shut down the RGB matrix.
    delete matrix;
    printf("\n");
    return 0;
}