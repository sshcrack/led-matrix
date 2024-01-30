#include "canvas.h"
#include "pixel_art.h"
#include "interrupt.h"
#include <sys/time.h>
#include "spdlog/spdlog.h"
#include "image.h"
#include "utils.h"
#include <vector>

#include <Magick++.h>
#include <tuple>
#include <future>

using namespace std;
using namespace spdlog;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

optional<vector<Magick::Image>> prefetch_images(Post* item, int height, int width) {
    item->fetch_link();
    if(!item->image.has_value() || !item->file_name.has_value()) {
        error("Could not load image {}", item->url);
        return nullopt;
    }

    string img_url = item->image.value();
    string file_name = item->file_name.value();

    // Downloading image first
    download_image(img_url, file_name);

    vector<Magick::Image> frames;
    string err_msg;
    if (!LoadImageAndScale(file_name.c_str(), width, height, false, false, &frames, &err_msg)) {
        error("Error loading image: {}", err_msg);
        return nullopt;
    }


    optional<vector<Magick::Image>> res;
    res = frames;

    return res;
}


void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix, vector<int>* total_pages) {
    const int height = matrix->height();
    const int width = matrix->width();


    int curr = (*total_pages)[0];
    total_pages->erase(total_pages->begin());
    total_pages->push_back(curr);

    auto posts = get_posts(curr);
    std::future<std::optional<std::vector<Magick::Image>>> next_post_frames = std::async(std::launch::async, &prefetch_images, &posts[0], height, width);

    for (size_t i = 0; i < posts.size(); i++) {
        if(interrupt_received)
            break;


        Post item = posts[i];        
        tmillis_t start_loading = GetTimeInMillis();

        optional<vector<Magick::Image>> frames_opt = next_post_frames.get();
        if(i != posts.size() -1) {
            next_post_frames = std::async(std::launch::async, &prefetch_images, &posts[i +1], height, width);
        }

        if(!frames_opt.has_value()) {
            error("Could not load image {}", item.url);
            continue;
        }


        FileInfo *file_info;
        vector<Magick::Image> frames = frames_opt.value();

        ImageParams params = ImageParams();
        if(frames.size() > 1) {
            params.anim_duration_ms = 15000;
        } else {
            params.wait_ms = 5000;
        }

        file_info = new FileInfo();
        file_info->params = params;
        file_info->content_stream = new rgb_matrix::MemStreamIO();
        file_info->is_multi_frame = frames.size() > 1;
        rgb_matrix::StreamWriter out(file_info->content_stream);
        for (const auto & img : frames) {
            tmillis_t delay_time_us;
            if (file_info->is_multi_frame) {
                delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
            } else {
                delay_time_us = file_info->params.wait_ms * 1000;  // single image.
            }
            if (delay_time_us <= 0) delay_time_us = 100 * 1000;  // 1/10sec
            StoreInStream(img, delay_time_us, true, canvas, &out);
        }


        info("Processing image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
        info("Showing animation for {} ({})", item.url, item.image);
        DisplayAnimation(file_info, matrix, canvas);

        item.file_name.and_then([](string file_name) {
            remove(file_name.c_str());
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
         !interrupt_received
         && GetTimeInMillis() < end_time_ms;
         ++k) {
        uint32_t delay_us = 0;
        while (!interrupt_received && GetTimeInMillis() <= end_time_ms
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