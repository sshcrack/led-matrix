#include "canvas.h"
#include "image.h"
#include "spdlog/spdlog.h"
#include "post.h"
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


void update_canvas(FrameCanvas *canvas, RGBMatrix *matrix) {
    const int height = matrix->height();
    const int width = matrix->width();

    auto curr_setting = config->get_curr();
    curr_setting.randomize();

    debug("Randomizing with total of {} image categories", curr_setting.images.size());
    for (const auto &img_category: curr_setting.images) {
        if(exit_canvas_update) {
            debug("Breaking");
            break;
        }

        debug("Getting next...");
        future<optional<Post>> next_img = async(launch::async, &ImageTypes::General::get_next_image, img_category);
        while(!exit_canvas_update) {
            debug("start loop");
            tmillis_t start_loading = GetTimeInMillis();

            optional<Post> fut = next_img.get();
            if(!fut.has_value())
                break;

            Post next_p = fut.value();
            next_img = async(launch::async, &ImageTypes::General::get_next_image, img_category);

            auto frames_opt = next_p.process_images(width, height);
            if(!frames_opt.has_value()) {
                error("Could not load image {}", next_p.get_image_url());
                //TODO Optimize here, exclude not loadable images
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
            info("Showing animation for {} ({})", next_p.get_filename(), next_p.get_image_url());
            DisplayAnimation(file_info, matrix, canvas);
            debug("End display");
        }
        debug("Flushing");
        img_category->flush();
        debug("Done");
    }
    debug("End of func");
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
            debug("Showing loop with {}", exit_canvas_update.load(memory_order_acq_rel));
            const tmillis_t anim_delay_ms =
                    override_anim_delay >= 0 ? override_anim_delay : delay_us / 1000;
            const tmillis_t start_wait_ms = GetTimeInMillis();
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                                   file->params.vsync_multiple);
            const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;
            SleepMillis(anim_delay_ms - time_already_spent);
        }
        debug("Rewind with {}", exit_canvas_update.load(memory_order_acq_rel));
        reader.Rewind();
    }
}