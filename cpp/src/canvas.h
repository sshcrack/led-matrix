#include "content-streamer.h"
#include "led-matrix.h"

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix, int page_end);


typedef int64_t tmillis_t;
static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct ImageParams {
    ImageParams() : anim_duration_ms(distant_future), wait_ms(1500),
                    anim_delay_ms(-1), loops(-1), vsync_multiple(1) {}
    tmillis_t anim_duration_ms;  // If this is an animation, duration to show.
    tmillis_t wait_ms;           // Regular image: duration to show.
    tmillis_t anim_delay_ms;     // Animation delay override.
    int loops;
    int vsync_multiple;
};

struct FileInfo {
    ImageParams params;      // Each file might have specific timing settings
    bool is_multi_frame{};
    rgb_matrix::StreamIO *content_stream{};
};


void DisplayAnimation(const FileInfo *file,
                      RGBMatrix *matrix, FrameCanvas *offscreen_canvas);