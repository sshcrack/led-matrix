#include "led-matrix.h"

#include <Magick++.h>
#include <magick/image.h>

void update_canvas(rgb_matrix::Canvas *canvas) {
    canvas->Clear();
    const int height = canvas->height();
    const int width = canvas->width();

    canvas->SetPixel(0, 0, 255,255, 255);
}