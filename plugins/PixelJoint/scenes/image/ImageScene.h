#pragma once

#include "led-matrix.h"
#include <optional>
#include <utility>
#include <future>
#include <shared/config/MainConfig.h>
#include <spdlog/spdlog.h>

#include "Scene.h"
#include "plugin/main.h"
#include "shared/utils/utils.h"
#include "shared/config/data.h"


static const tmillis_t distant_future = (1LL << 40); // that is a while.
struct ImageParams {
    ImageParams() : duration_ms(distant_future), vsync_multiple(1) {
    }

    tmillis_t duration_ms; // If this is an animation, duration to show.
    int vsync_multiple;
};

struct FileInfo {
    ImageParams params; // Each file might have specific timing settings
    bool is_multi_frame{};
    rgb_matrix::StreamIO *content_stream{};

    ~FileInfo() {
        spdlog::trace("Deleting content stream");
        std::flush(std::cout);
        delete content_stream;
    }
};

struct CurrAnimation {
    rgb_matrix::StreamReader reader;
    std::unique_ptr<FileInfo, void(*)(FileInfo *)> file;
    const tmillis_t end_time_ms;

    CurrAnimation(const rgb_matrix::StreamReader &reader,
                  const tmillis_t end_time_ms,
                  std::unique_ptr<FileInfo, void(*)(FileInfo *)> file): reader(reader), file(std::move(file)),
                                                                        end_time_ms(end_time_ms) {
    }

    ~CurrAnimation() {
        spdlog::trace("Deleting animation");
        std::flush(std::cout);
    }
};

struct ImageInfo {
    vector<Magick::Image> frames;
    std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post> > post;
};

const std::string PROVIDER_DEFAULT = R"(
[
{
  "type": "pages",
  "arguments": {
    "begin": 0,
    "end": -1
  }
}
])";

class ImageScene final : public Scenes::Scene {
    std::optional<std::unique_ptr<CurrAnimation, void(*)(CurrAnimation *)> > curr_animation;
    std::vector<std::shared_ptr<ImageProviders::General> > providers;

    uint curr_category = 0;
    std::atomic<bool> is_exiting{false};
    std::atomic<bool> has_image{false};

    optional<std::future<expected<optional<ImageInfo>, string> > > next_img;


    bool DisplayAnimation(RGBMatrix *matrix);

    expected<std::unique_ptr<CurrAnimation, void(*)(CurrAnimation *)>, string>
    get_next_anim(RGBMatrix *matrix, int recursiveness);

    static expected<optional<ImageInfo>, string>
    get_next_image(const std::shared_ptr<ImageProviders::General> &category, int width, int height,
                   const atomic<bool> &is_exiting);

    std::unique_ptr<FileInfo, void(*)(FileInfo *)> GetFileInfo(const vector<Magick::Image> &frames,
                                                               FrameCanvas *canvas) const;

    PropertyPointer<tmillis_t> image_display_duration = MAKE_PROPERTY("display_duration", tmillis_t, 15000);
    PropertyPointer<nlohmann::json> json_providers = MAKE_PROPERTY("providers", nlohmann::json,
                                                                   nlohmann::json::parse(PROVIDER_DEFAULT));

public:
    /// Return true if scene should continue rendering
    bool render(RGBMatrix *matrix) override;

    [[nodiscard]] string get_name() const override;

    void register_properties() override {
        add_property(image_display_duration);
        add_property(json_providers);
    }

    using Scene::Scene;

    ~ImageScene() override;

    void load_properties(const nlohmann::json &j) override;
};

class ImageSceneWrapper final : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
};
