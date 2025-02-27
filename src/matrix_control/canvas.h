#pragma once

#include "led-matrix.h"
#include "shared/post.h"
#include "content-streamer.h"
#include "shared/utils/utils.h"
#include "shared/utils/canvas_image.h"
#include <vector>

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

FrameCanvas *update_canvas(RGBMatrix *matrix, FrameCanvas *pCanvas);
