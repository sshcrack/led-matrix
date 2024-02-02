#include "canvas.h"
#include "pixel_art.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include "image.h"
#include "../utils.h"
#include "../shared.h"
#include <vector>

#include <Magick++.h>
#include <future>

using namespace std;
using namespace spdlog;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;


string root_dir = "images/";

optional<vector<Magick::Image>> prefetch_images(Post *item, int height, int width) {
    if (!filesystem::exists(root_dir)) {
        try {
            auto res = filesystem::create_directory(root_dir);
            if(!res) {
                error("Could not create directory at {}.", root_dir);
                exit(-1);
            }
        } catch (exception& ex) {
            error("Could not create directory at {} with exception: {}", root_dir, ex.what());
            exit(-1);
        }
    }

    tmillis_t start_loading = GetTimeInMillis();
    item->fetch_link();
    if (!item->image.has_value() || !item->file_name.has_value()) {
        error("Could not load image {}", item->url);
        return nullopt;
    }

    string img_url = item->image.value();
    string base_name = item->file_name.value();
    string path = root_dir + base_name;


    // Downloading image first
    if (!filesystem::exists(path + "0"))
        download_image(img_url, path);

    vector<Magick::Image> frames;
    string err_msg;

    bool contain_img = true;
    if (!LoadImageAndScale(path, width, height, true, true, contain_img, &frames, &err_msg)) {
        error("Error loading image: {}", err_msg);
        return nullopt;
    }


    optional<vector<Magick::Image>> res;
    res = frames;
    debug("Loading/Scaling Image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);

    return res;
}


void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix, vector<int> *total_pages) {
    const int height = matrix->height();
    const int width = matrix->width();


    int curr = (*total_pages)[0];
    total_pages->erase(total_pages->begin());
    total_pages->push_back(curr);

    auto posts = get_posts(curr);
    future<optional<vector<Magick::Image>>> next_post_frames = async(launch::async,
                                                                     &prefetch_images, &posts[0],
                                                                     height, width);

    for (size_t i = 0; i < posts.size(); i++) {
        if (exit_canvas_update)
            break;


        Post *item = &posts[i];
        tmillis_t start_loading = GetTimeInMillis();

        optional<vector<Magick::Image>> frames_opt = next_post_frames.get();
        if (i != posts.size() - 1)
            next_post_frames = async(launch::async, &prefetch_images, &posts[i + 1], height, width);

        if (!frames_opt.has_value()) {
            error("Could not load image {}", item->url);
            continue;
        }


        FileInfo *file_info;
        vector<Magick::Image> frames = frames_opt.value();

        ImageParams params = ImageParams();
        if (frames.size() > 1) {
            params.anim_duration_ms = 15000;
        } else {
            params.wait_ms = 5000;
        }

        file_info = new FileInfo();
        file_info->params = params;
        file_info->content_stream = new rgb_matrix::MemStreamIO();
        file_info->is_multi_frame = frames.size() > 1;

        rgb_matrix::StreamWriter out(file_info->content_stream);
        for (const auto &img: frames) {
            tmillis_t delay_time_us;
            if (file_info->is_multi_frame) {
                delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
            } else {
                delay_time_us = file_info->params.wait_ms * 1000;  // single image.
            }
            if (delay_time_us <= 0) delay_time_us = 100 * 1000;  // 1/10sec
            StoreInStream(img, delay_time_us, true, canvas, &out);
        }


        info("Showing image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
        info("Showing animation for {} ({})", item->url, item->image.value_or("(NO_URL)"));
        DisplayAnimation(file_info, matrix, canvas);

        item->file_name.and_then([](const string &file_name) {
            remove(file_name.c_str());

            return optional<string>();
        });
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