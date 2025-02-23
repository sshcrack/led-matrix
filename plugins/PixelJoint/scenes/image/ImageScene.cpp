#include "ImageScene.h"
#include "spdlog/spdlog.h"
#include "shared/utils/utils.h"
#include "shared/utils/shared.h"
#include "shared/interrupt.h"
#include "shared/utils/canvas_image.h"
#include <vector>

#include <Magick++.h>
#include <future>
#include <shared/plugin_loader/loader.h>

using namespace std;
using namespace spdlog;

using rgb_matrix::StreamReader;


bool ImageScene::DisplayAnimation(RGBMatrix *matrix) {
    auto curr = &curr_animation.value();
    const tmillis_t start_wait_ms = GetTimeInMillis();

    if (skip_image || GetTimeInMillis() > curr->get()->end_time_ms) {
        this->curr_animation.reset();
        return true;
    }

    uint32_t delay_us = 0;
    //TODO I think this isn't safe like at all
    if (const auto reader = &curr_animation->get()->reader; !reader->GetNext(offscreen_canvas, &delay_us)) {
        reader->Rewind();
        if (!reader->GetNext(offscreen_canvas, &delay_us)) {
            this->curr_animation.reset();
            return false;
        }
    }

    const tmillis_t anim_delay_ms = delay_us / 1000;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnusedValue"
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
#pragma clang diagnostic pop

    const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;

    tmillis_t to_wait = anim_delay_ms - time_already_spent;
    while (to_wait > 0) {
        if (interrupt_received || exit_canvas_update) {
            this->curr_animation.reset();
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


bool ImageScene::render(RGBMatrix *matrix) {
    if (!this->curr_animation.has_value()) {
        // This is only called once on first render
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

Post *get_pointer_raw(std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post> > &post) {
    if (holds_alternative<std::unique_ptr<Post, void (*)(Post *)> >(post)) {
        return get<0>(post).get();
    }

    return get<1>(post).get();
}

expected<std::unique_ptr<CurrAnimation, void(*)(CurrAnimation *)>, string>
ImageScene::get_next_anim(RGBMatrix *matrix, int recursiveness) {
    // NOLINT(*-no-recursion)

    if (recursiveness > 10) {
        return unexpected("Too many recursions");
    }

    if (this->curr_category >= providers.size()) {
        this->curr_category = 0;
    }

    int width = matrix->width();
    int height = matrix->height();


    auto img_category = providers[this->curr_category];
    if (!this->next_img.has_value() || !this->next_img->valid()) {
        this->next_img = async(launch::async, get_next_image, img_category, width, height, std::ref(is_exiting));
    }

    tmillis_t start_loading = GetTimeInMillis();

    optional<expected<optional<ImageInfo>, string> > info_res_opt = nullopt;
    try {
        info_res_opt = std::move(next_img->get());
        if (!info_res_opt.value().has_value()) {
            warn("Could not get next image. Trying again. Error was: {}", info_res_opt.value().error());
            return get_next_anim(matrix, recursiveness + 1);
        }
    } catch (const std::bad_alloc &e) {
        error("Memory allocation failed: {}. Trying next image.", e.what());
        return get_next_anim(matrix, recursiveness + 1);
    }

    auto info_res = std::move(info_res_opt.value());
    auto info_opt = std::move(info_res.value());
    if (!info_opt.has_value()) {
        // No images left, new category
        img_category->flush();
        this->curr_category++;

        debug("No images left. Flushing and moving onto next category.");
        return get_next_anim(matrix, recursiveness + 1);
    }

    next_img = async(launch::async, get_next_image, img_category, width, height, std::ref(is_exiting));
    auto [frames, post] = std::move(info_opt.value());

    auto raw_ptr = get_pointer_raw(post);
    auto filename = raw_ptr->get_filename();
    auto image_url = raw_ptr->get_image_url();

    auto file = GetFileInfo(frames, offscreen_canvas);
    info("Loaded p_info for {} ({})", filename, image_url);

    info("Loading image took {}s.", (GetTimeInMillis() - start_loading) / 1000.0);
    const tmillis_t duration_ms = file->params.duration_ms;

    StreamReader reader(file->content_stream);
    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;

    std::unique_ptr<CurrAnimation, void(*)(CurrAnimation *)> res(
        new CurrAnimation(reader, end_time_ms, std::move(file)),
        [](CurrAnimation *anim) {
            delete anim;
        });

    return res;
}


expected<optional<ImageInfo>, string>
ImageScene::get_next_image(const std::shared_ptr<ImageProviders::General> &category, const int width,
                           const int height, const atomic<bool> &is_exiting) {
    auto post_res = category->get_next_image();
    if (!post_res.has_value()) {
        return unexpected(post_res.error());
    }

    auto post_opt = std::move(post_res.value());
    if (!post_opt.has_value()) {
        return std::nullopt;
    }

    auto post = std::move(post_opt.value());
    const auto raw_post = get_pointer_raw(post);
    auto frames_opt = raw_post->process_images(width, height);

    if (!frames_opt.has_value()) {
        const auto image_url = raw_post->get_image_url();

        return unexpected("Could not load image " + image_url);
    }

    if (is_exiting.load()) {
        debug("Exiting image scene...");
        return nullopt;
    }

    return ImageInfo{
        .frames = std::move(frames_opt.value()),
        .post = std::move(post)
    };
}

std::unique_ptr<FileInfo, void(*)(FileInfo *)> ImageScene::GetFileInfo(const vector<Magick::Image> &frames,
                                                                       FrameCanvas *canvas) const {
    auto params = ImageParams();
    params.duration_ms = image_display_duration->get();

    std::unique_ptr<FileInfo, void(*)(FileInfo *)> file_info = {
        new FileInfo(),
        [](FileInfo *file) {
            delete file;
        }
    };

    file_info->params = params;
    file_info->content_stream = new rgb_matrix::MemStreamIO();
    file_info->is_multi_frame = frames.size() > 1;

    rgb_matrix::StreamWriter out(file_info->content_stream);
    for (const auto &img: frames) {
        tmillis_t delay_time_us;
        if (file_info->is_multi_frame) {
            delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
        } else {
            delay_time_us = file_info->params.duration_ms * 1000; // single image.
        }
        if (delay_time_us <= 0) delay_time_us = 100 * 1000; // 1/10sec
        StoreInStream(img, delay_time_us, true, canvas, &out);
    }

    return file_info;
}


string ImageScene::get_name() const {
    return "image_scene";
}

void ImageScene::load_properties(const nlohmann::json &j) {
    Scene::load_properties(j);

    auto is_array = json_providers.get()->get().is_array();
    if (!is_array)
        throw std::runtime_error("Providers of image scene " + this->get_name() + " must be an array");

    auto arr = json_providers->get();
    if (arr.empty())
        throw std::runtime_error("No image providers given for image scene " + this->get_name());

    auto pl_providers = Plugins::PluginManager::instance()->get_image_providers();
    for (const auto &provider_json: arr) {
        providers.push_back(ImageProviders::General::from_json(provider_json));
    }
}

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> ImageSceneWrapper::create() {
    return {
        new ImageScene(), [](Scenes::Scene *scene) {
            delete scene;
        }
    };
}
