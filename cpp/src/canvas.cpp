#include "led-matrix.h"
#include "pixel_art.h"

#include <random>
#include <Magick++.h>
#include <magick/image.h>

void update_canvas(rgb_matrix::Canvas *canvas, int page_end) {
    //canvas->Clear();
    const int height = canvas->height();
    const int width = canvas->width();


    int start = 1;
    int end = 81;

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distr(start, end); // define the range
    auto posts = get_posts(distr(gen));
}