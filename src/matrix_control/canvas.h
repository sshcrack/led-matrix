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


static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct ImageParams {
    ImageParams() : duration_ms(distant_future), wait_ms(1500),
                    anim_delay_ms(-1), vsync_multiple(1) {}
    tmillis_t duration_ms;  // If this is an animation, duration to show.
    tmillis_t wait_ms;           // Regular image: duration to show.
    tmillis_t anim_delay_ms;     // Animation delay override.
    int vsync_multiple;
};

struct FileInfo {
    ImageParams params;      // Each file might have specific timing settings
    bool is_multi_frame{};
    rgb_matrix::StreamIO *content_stream{};
};


FileInfo GetFileInfo(tuple<vector<Magick::Image>, Post> p_info, FrameCanvas *canvas);
void DisplayAnimation(const FileInfo *file,
                      RGBMatrix *matrix, FrameCanvas *offscreen_canvas);