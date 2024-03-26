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


FileInfo GetFileInfo(tuple<vector<Magick::Image>, Post> p_info, FrameCanvas *canvas) {
    auto frames = get<0>(p_info);
    auto post = get<1>(p_info);

    ImageParams params = ImageParams();
    if (frames.size() > 1) {
        params.anim_duration_ms = 15000;
    } else {
        params.wait_ms = 5000;
    }

    FileInfo file_info = FileInfo();
    file_info.params = params;
    file_info.content_stream = new rgb_matrix::MemStreamIO();
    file_info.is_multi_frame = frames.size() > 1;

    rgb_matrix::StreamWriter out(file_info.content_stream);
    for (const auto &img: frames) {
        tmillis_t delay_time_us;
        if (file_info.is_multi_frame) {
            delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
        } else {
            delay_time_us = file_info.params.wait_ms * 1000;  // single image.
        }
        if (delay_time_us <= 0) delay_time_us = 100 * 1000;  // 1/10sec
        StoreInStream(img, delay_time_us, true, canvas, &out);
    }


    info("Loaded p_info for {} ({})", post.get_filename(), post.get_image_url());
    return file_info;
}

expected<optional<tuple<vector<Magick::Image>, Post>>, string> get_next_image(ImageTypes::General* category, int width, int height) {
    debug("Getting next");
    auto post = category->get_next_image();
    debug("Done");
    if(!post.has_value()) {
        debug("Unexpected");
        return unexpected("End of images for category");
    }


    debug("Frame opt");
    auto frames_opt = post->process_images(width, height);
    debug("Done");
    if(!frames_opt.has_value()) {
        error("Could not load image {}", post->get_image_url());
        //TODO Optimize here, exclude not loadable images
        return nullopt;
    }

    debug("Return val");
    return make_tuple(frames_opt.value(), post.value());
}


void DisplayCurrentSong(const FileInfo *file, SpotifyState* state,
                        RGBMatrix *matrix, FrameCanvas *offscreen_canvas) {
    const tmillis_t duration_ms = (file->params.wait_ms);
    rgb_matrix::StreamReader reader(file->content_stream);


    long progress_ms = state->get_progress_ms();
    long duration = state->get_track().get_duration();

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
            if(track_loc > duration) {
                break;
            }

            float progress = (float) track_loc / (float) duration;
            int line_width = matrix->width() * progress;

            DrawLine(offscreen_canvas,0,0,line_width,0,rgb_matrix::Color(255,255,255));
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                                   file->params.vsync_multiple);


            const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;

            SleepMillis(time_already_spent - 1000);
        }
        reader.Rewind();
    }
}


void ShowSpotifyChange(RGBMatrix *matrix, FrameCanvas *canvas) {
    info("Showing spotify song change");
    auto temp = spotify->get_currently_playing();
    if(!temp.has_value()) {
        return;
    }

    auto state = temp.value();
    auto temp2 = state.get_track().get_cover();
    if(!temp2.has_value()) {
        return;
    }

    auto cover = temp2.value();
    char out_file[] = "/tmp/spotify_cover.XXXXXX";
    int temp_fd = mkstemp(out_file);

    if(temp_fd == -1) {
        error("Could not create temporary file for spotify cover");
        return;
    }

    debug("Downloading");
    download_image(cover, out_file);

    vector<Magick::Image> frames;
    string err_msg;

    LoadImageAndScale(out_file, matrix->width(), matrix->height(), true, true, false, &frames, &err_msg);
    if(!err_msg.empty()) {
        error("Error loading image: {}", err_msg);
        return;
    }

    close(temp_fd);

    FileInfo file_info = FileInfo();
    file_info.params.wait_ms = 5000;
    file_info.content_stream = new rgb_matrix::MemStreamIO();
    file_info.is_multi_frame = frames.size() > 1;


    debug("Showing and storing");
    rgb_matrix::StreamWriter out(file_info.content_stream);
    for (const auto &img: frames) {
        StoreInStream(img, 100 * 1000, true, canvas, &out);
    }

    DisplayCurrentSong(&file_info, &state, matrix, canvas);
}

void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix) {
    debug("Start");
    const int height = matrix->height();
    const int width = matrix->width();

    debug("Curr");
    auto curr_setting = config->get_curr();
    curr_setting.randomize();

    debug("Randomizing with total of {} image categories", curr_setting.categories.size());
    for (const auto &img_category: curr_setting.categories) {
        if(exit_canvas_update) {
            break;
        }

    debug("Async");
        auto next_img = async(launch::async, get_next_image, img_category, width, height);
        while(!exit_canvas_update) {
            debug("Checking for spotify change");
            if(spotify->has_changed()) {
                debug("Showing changed");
                ShowSpotifyChange(matrix, canvas);
            }

            tmillis_t start_loading = GetTimeInMillis();

            debug("Get");
            auto info_opt = next_img.get();
            debug("check val");
            if(!info_opt.has_value()) {
                debug("Break");
                break;
            }

            debug("New");
            next_img = async(launch::async, get_next_image, img_category, width, height);
            debug("Get 2");
            auto val = info_opt.value();
            if(!val.has_value()) {
                debug("There was an error with the previous image. Waiting for new image to download and process...");
                continue;
            }

            debug("Val file");
            auto file_info = GetFileInfo(val.value(), canvas);

            info("Loading image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
            DisplayAnimation(&file_info, matrix, canvas);
        }
        img_category->flush();
    }
}

void DisplayAnimation(const FileInfo *file,
                      RGBMatrix *matrix, FrameCanvas *offscreen_canvas) {
    const tmillis_t duration_ms = (file->is_multi_frame
                                   ? file->params.anim_duration_ms
                                   : file->params.wait_ms);
    rgb_matrix::StreamReader reader(file->content_stream);
    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;
    const tmillis_t override_anim_delay = file->params.anim_delay_ms;
    for (int k = 0;
         !exit_canvas_update && !skip_image
         && GetTimeInMillis() < end_time_ms; ++k) {
        uint32_t delay_us = 0;
        while (!exit_canvas_update && !skip_image
               && GetTimeInMillis() <= end_time_ms
               && reader.GetNext(offscreen_canvas, &delay_us)) {
            const tmillis_t anim_delay_ms =
                    override_anim_delay >= 0 ? override_anim_delay : delay_us / 1000;
            const tmillis_t start_wait_ms = GetTimeInMillis();
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                                   file->params.vsync_multiple);
            const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;
            SleepMillis(anim_delay_ms - time_already_spent);
        }
        reader.Rewind();
    }
}