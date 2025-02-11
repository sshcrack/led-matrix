#include "ImageScene.h"
#include "spdlog/spdlog.h"
#include "shared/utils/utils.h"
#include "shared/utils/shared.h"
#include "shared/utils/image_fetch.h"
#include "shared/interrupt.h"
#include "shared/utils/canvas_image.h"
#include <vector>

#include <Magick++.h>
#include <future>

using namespace std;
using namespace spdlog;

using rgb_matrix::StreamReader;


bool ImageScene::DisplayAnimation(rgb_matrix::RGBMatrix *matrix) {
    auto curr = &curr_animation.value();
    const tmillis_t start_wait_ms = GetTimeInMillis();

    if (skip_image || GetTimeInMillis() > curr->end_time_ms) {
        this->curr_animation.reset();
        return false;
    }

    uint32_t delay_us = 0;
    //TODO I think this isn't safe like at all
    auto reader = &(curr_animation->reader);
    if (!reader->GetNext(offscreen_canvas, &delay_us)) {
        reader->Rewind();
        if (!reader->GetNext(offscreen_canvas, &delay_us)) {
            return false;
        }
    }

    const tmillis_t anim_delay_ms = delay_us / 1000;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedValue"
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                           curr->file->params.vsync_multiple);
#pragma clang diagnostic pop

    const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;

    tmillis_t to_wait = anim_delay_ms - time_already_spent;
    while (to_wait > 0) {
        if (interrupt_received) {
            return false;
        }

        if (to_wait < 250) {
            SleepMillis(to_wait);
            break;
        }

        SleepMillis(250);
        to_wait -= 250;
    }

    return true;
}


bool ImageScene::render(rgb_matrix::RGBMatrix *matrix) {
    if (!this->curr_animation.has_value()) {
        debug("Getting next animation");
        auto res = get_next_anim(matrix, 0);
        if (!res.has_value()) {
            error("Error getting next animation: {}", res.error());
            return false;
        }

        this->curr_animation.emplace(std::move(res.value()));
    }

    return DisplayAnimation(matrix);
}

Post *get_pointer_raw(std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post>> &post) {
    if (holds_alternative<std::unique_ptr<Post, void (*)(Post *)>>(post)) {
        return get < 0 > (post).get();
    } else {
        return get < 1 > (post).get();
    }
}

expected<CurrAnimation, string>
ImageScene::get_next_anim(rgb_matrix::RGBMatrix *matrix, int recursiveness) { // NOLINT(*-no-recursion)
    if (recursiveness > 10) {
        return unexpected("Too many recursions");
    }

    if (!this->curr_preset.has_value()) {
        auto temp = config->get_curr();
        temp.randomize();

        debug("Randomizing with total of {} image providers", temp.providers.size());
        auto p = CurrPreset(temp);
        this->curr_preset.emplace(p);
    }

    auto categories = this->curr_preset->preset.providers;
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
        return get_next_anim(matrix, recursiveness + 1);
    }

    next_img = async(launch::async, ImageScene::get_next_image, img_category, width, height);
    auto val_opt = std::move(info_opt.value());
    if (!val_opt.has_value()) {
        debug("There was an error with the previous image. Waiting for new image to download and process...");
        return get_next_anim(matrix, recursiveness + 1);
    }

    auto val = std::move(val_opt.value());

    auto raw_ptr = get_pointer_raw(val.post);
    auto filename = raw_ptr->get_filename();
    auto image_url = raw_ptr->get_filename();

    auto file = GetFileInfo(val.frames, offscreen_canvas);
    info("Loaded p_info for {} ({})", filename, image_url);

    info("Loading image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
    const tmillis_t duration_ms = file->params.duration_ms;

    rgb_matrix::StreamReader reader(file->content_stream);
    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;

    return CurrAnimation(std::move(file), reader, end_time_ms);
}


expected<optional<ImageInfo>, string>
ImageScene::get_next_image(std::shared_ptr<ImageProviders::General> category, int width, int height) {
    auto post_opt = category->get_next_image();
    if (!post_opt.has_value()) {
        return unexpected("End of images for category");
    }


    auto post = std::move(post_opt.value());
    auto raw_post = get_pointer_raw(post);
    auto frames_opt = raw_post->process_images(width, height);

    if (!frames_opt.has_value()) {
        auto image_url = raw_post->get_image_url();

        error("Could not load image {}", image_url);

//TODO Optimize here, exclude not loadable images
        return nullopt;
    }

    return ImageInfo{
            .frames = std::move(frames_opt.value()),
            .post = std::move(post)
    };
}

std::unique_ptr<FileInfo, void (*)(FileInfo *)>
ImageScene::GetFileInfo(vector<Magick::Image> frames, FrameCanvas *canvas) {
    auto params = ImageParams();
    params.duration_ms = 15000;

    auto file_info = new FileInfo();
    file_info->params = params;
    file_info->content_stream = new rgb_matrix::MemStreamIO();
    file_info->is_multi_frame = frames.size() > 1;

    rgb_matrix::StreamWriter out(file_info->content_stream);
    for (const auto &img: frames) {
        tmillis_t delay_time_us;
        if (file_info->is_multi_frame) {
            delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
        } else {
            delay_time_us = file_info->params.duration_ms * 1000;  // single image.
        }
        if (delay_time_us <= 0) delay_time_us = 100 * 1000;  // 1/10sec
        StoreInStream(img, delay_time_us, true, canvas, &out);
    }

    return {file_info, [](FileInfo *info) {
        delete info;
    }};
}


string ImageScene::get_name() const {
    return "image_scene";
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> ImageSceneWrapper::create() {
    return {new ImageScene(), [](Scenes::Scene *scene) {
        delete scene;
    }};
}
