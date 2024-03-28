//
// Created by hendrik on 3/28/24.
//

#include "PresetScene.h"
#include "../image.h"
#include "spdlog/spdlog.h"
#include "../post.h"
#include "../../spotify/shared_spotify.h"
#include "../../utils/utils.h"
#include "../../utils/shared.h"
#include "../pixel_art.h"
#include <vector>

#include <Magick++.h>
#include <future>

using namespace std;
using namespace spdlog;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

expected<optional<ImageInfo>, string>
get_next_image(ImageTypes::General *category, int width, int height) {
    auto post = category->get_next_image();
    if (!post.has_value()) {
        return unexpected("End of images for category");
    }


    auto frames_opt = post->process_images(width, height);
    if (!frames_opt.has_value()) {
        error("Could not load image {}", post->get_image_url());
//TODO Optimize here, exclude not loadable images
        return nullopt;
    }

    return make_tuple(frames_opt.value(), post.value());
}


void PresetScene::DisplayAnimation(RGBMatrix *matrix) {
    auto curr = &curr_animation.value();

    if(skip_image || GetTimeInMillis() > curr->end_time_ms) {
        this->curr_animation.reset();
        return;
    }

    uint32_t delay_us = 0;
    auto reader = curr_animation->reader;
    if(!reader.GetNext(offscreen_canvas, &delay_us)) {
        reader.Rewind();
        if(!reader.GetNext(offscreen_canvas, &delay_us)) {
            return;
        }
    }

    const tmillis_t anim_delay_ms =
            curr->override_anim_delay >= 0 ? curr->override_anim_delay : delay_us / 1000;
    const tmillis_t start_wait_ms = GetTimeInMillis();

    #pragma clang diagnostic push
    #pragma ide diagnostic ignored "UnusedValue"
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                 curr->file.params.vsync_multiple);
    #pragma clang diagnostic pop

    const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;

    debug("Sleeping for {}", anim_delay_ms - time_already_spent);
    SleepMillis(anim_delay_ms - time_already_spent);
}


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


void PresetScene::tick(RGBMatrix *matrix) {
    if(!this->curr_animation.has_value()) {
        auto res = get_next_anim(matrix, 0);
        if(!res.has_value()) {
            error("Error getting next animation: {}", res.error());
            return;
        }

        this->curr_animation.emplace(res.value());
    }

    DisplayAnimation(matrix);
}

expected<CurrAnimation, string> PresetScene::get_next_anim(RGBMatrix *matrix, int recursiveness) {
    if(recursiveness > 10) {
        return unexpected("Too many recursions");
    }

    if (!this->curr_preset.has_value()) {
        auto temp = config->get_curr();
        temp.randomize();

        debug("Randomizing with total of {} image categories", temp.categories.size());
        auto p = CurrPreset(temp);
        this->curr_preset.emplace(p);
    }

    auto categories = this->curr_preset->preset.categories;
    if (this->curr_preset->curr_category >= categories.size()) {
        this->curr_preset->curr_category = 0;
    }

    int width = matrix->width();
    int height = matrix->height();


    auto img_category = categories[this->curr_preset->curr_category];
    if (!this->next_img.has_value()) {
        this->next_img = async(launch::async, get_next_image, img_category, width, height);
    }

    tmillis_t start_loading = GetTimeInMillis();

    auto info_opt = next_img->get();
    if (!info_opt.has_value()) {
        // No images left, new category
        img_category->flush();
        this->curr_preset->curr_category++;

        debug("Flushing category, moving to next one...");
        return get_next_anim(matrix, recursiveness +1);
    }

    next_img = async(launch::async, get_next_image, img_category, width, height);
    auto val = info_opt.value();
    if (!val.has_value()) {
        debug("There was an error with the previous image. Waiting for new image to download and process...");
        return get_next_anim(matrix, recursiveness +1);
    }

    auto file = GetFileInfo(val.value(), offscreen_canvas);

    info("Loading image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
    const tmillis_t duration_ms = (file.is_multi_frame
                                   ? file.params.anim_duration_ms
                                   : file.params.wait_ms);

    rgb_matrix::StreamReader reader(file.content_stream);
    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;
    const tmillis_t override_anim_delay = file.params.anim_delay_ms;

    return CurrAnimation(file, reader, end_time_ms, override_anim_delay);
}