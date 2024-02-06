#include "canvas.h"
#include "image.h"
#include "spdlog/spdlog.h"
#include "post.h"
#include "../utils/utils.h"
#include "../utils/shared.h"
#include <vector>

#include <Magick++.h>
#include <future>

using namespace std;
using namespace spdlog;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

optional<tuple<vector<Magick::Image>, Post>> get_next_image(ImageTypes::General* category, int width, int height) {
    auto post = category->get_next_image();
    if(!post.has_value())
        return nullopt;


    auto frames_opt = post->process_images(width, height);
    if(!frames_opt.has_value()) {
        error("Could not load image {}", post->get_image_url());
        //TODO Optimize here, exclude not loadable images
        return nullopt;
    }

    return make_tuple(frames_opt.value(), post.value());
}

void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix) {
    const int height = matrix->height();
    const int width = matrix->width();

    auto curr_setting = config->get_curr();
    curr_setting.randomize();

    debug("Randomizing with total of {} image categories", curr_setting.categories.size());
    for (const auto &img_category: curr_setting.categories) {
        if(exit_canvas_update) {
            break;
        }

        auto next_img = async(launch::async, get_next_image, img_category, width, height);
        while(!exit_canvas_update) {
            tmillis_t start_loading = GetTimeInMillis();

            auto info_opt = next_img.get();
            if(!info_opt.has_value())
                break;

            next_img = async(launch::async, get_next_image, img_category, width, height);

            FileInfo *file_info;
            auto tuple = info_opt.value();
            auto frames = get<0>(tuple);
            auto post = get<1>(tuple);

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
            info("Showing animation for {} ({})", post.get_filename(), post.get_image_url());
            DisplayAnimation(file_info, matrix, canvas);
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