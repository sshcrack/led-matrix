#pragma once

#include "led-matrix.h"
#include "shared/matrix/post.h"
#include "content-streamer.h"
#include "shared/matrix/utils/utils.h"
#include "shared/matrix/utils/canvas_image.h"
#include <vector>

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrixBase;
using rgb_matrix::StreamReader;

FrameCanvas *update_canvas(RGBMatrixBase * matrix, FrameCanvas *pCanvas);
