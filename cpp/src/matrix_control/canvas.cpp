#include "canvas.h"
#include "image.h"
#include "spdlog/spdlog.h"
#include "post.h"
#include "../spotify/shared_spotify.h"
#include "../utils/utils.h"
#include "../utils/shared.h"
#include "pixel_art.h"
#include <vector>

#include <Magick++.h>
#include <future>

using namespace std;
using namespace spdlog;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;


void DisplayCurrentSong(const FileInfo *file, SpotifyState *state,
                        RGBMatrix *matrix, FrameCanvas *offscreen_canvas) {
    const tmillis_t duration_ms = (file->params.wait_ms);
    rgb_matrix::StreamReader reader(file->content_stream);


    long progress_ms = state->get_progress_ms();
    long duration = state->get_track().get_duration();
    float flash_duration = 5000;

    int w = matrix->width() - 1;
    int h = matrix->height() - 1;

    const tmillis_t start_time = GetTimeInMillis();

    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;
    for (int k = 0;
         !exit_canvas_update
         && GetTimeInMillis() < end_time_ms; ++k) {
        uint32_t delay_us = 0;
        while (!exit_canvas_update
               && GetTimeInMillis() <= end_time_ms
               && reader.GetNext(offscreen_canvas, &delay_us)) {
            const tmillis_t start_wait_ms = GetTimeInMillis();

            tmillis_t time_spent = GetTimeInMillis() - start_time;
            tmillis_t track_loc = progress_ms + time_spent;
            if (track_loc > duration) {
                break;
            }

            double brightness = abs(sin(M_PI * ((float) time_spent / flash_duration)));
            uint8_t p_brightness = brightness * 255;

            auto color = rgb_matrix::Color(0, p_brightness, 0);

            // Bottom / Top
            DrawLine(offscreen_canvas, 0, 0, w, 0, color);
            DrawLine(offscreen_canvas, 0, h, w, h, color);


            // Left / Right
            DrawLine(offscreen_canvas, 0, 0, 0, h, color);
            DrawLine(offscreen_canvas, w, 0, w, h, color);


            /*
            float progress = (float) track_loc / (float) duration;
            int line_width = matrix->width() * progress;


            DrawLine(offscreen_canvas, 0, 0, line_width, 0, rgb_matrix::Color(255, 255, 255));
            */
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                                   file->params.vsync_multiple);


            const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;
            if (time_already_spent > 100)
                continue;

            SleepMillis(100 - time_already_spent);
        }
        reader.Rewind();
    }
}


void ShowSpotifyChange(RGBMatrix *matrix, FrameCanvas *canvas) {
    info("Showing spotify song change");
    auto temp = spotify->get_currently_playing();
    if (!temp.has_value()) {
        return;
    }

    auto state = temp.value();
    auto temp2 = state.get_track().get_cover();
    if (!temp2.has_value()) {
        return;
    }


    auto cover = temp2.value();
    string out_file = "/tmp/spotify_cover." + state.get_track().get_id() + ".jpg";

    if (!std::filesystem::exists(out_file)) {
        debug("Downloading");
        download_image(cover, out_file);
    }

    vector<Magick::Image> frames;
    string err_msg;

    LoadImageAndScale(out_file, matrix->width(), matrix->height(), true, true, false, &frames, &err_msg);
    if (!err_msg.empty()) {
        error("Error loading image: {}", err_msg);
        return;
    }

    FileInfo file_info = FileInfo();
    file_info.params.wait_ms = 15000;
    file_info.content_stream = new rgb_matrix::MemStreamIO();
    file_info.is_multi_frame = frames.size() > 1;


    debug("Showing and storing");
    rgb_matrix::StreamWriter out(file_info.content_stream);
    for (const auto &img: frames) {
        StoreInStream(img, 100 * 1000, true, canvas, &out);
    }

    DisplayCurrentSong(&file_info, &state, matrix, canvas);
}

void update_canvas(RGBMatrix *matrix) {

}
