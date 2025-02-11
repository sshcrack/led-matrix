#pragma once

#include "led-matrix.h"
#include <optional>
#include <utility>
#include <future>
#include "Scene.h"
#include "plugin/main.h"
#include "shared/utils/utils.h"
#include "shared/config/data.h"


static const tmillis_t distant_future = (1LL << 40); // that is a while.
struct ImageParams {
    ImageParams() : duration_ms(distant_future), vsync_multiple(1) {}

    tmillis_t duration_ms;  // If this is an animation, duration to show.
    int vsync_multiple;
};

struct FileInfo {
    ImageParams params;      // Each file might have specific timing settings
    bool is_multi_frame{};
    rgb_matrix::StreamIO *content_stream{};

    ~FileInfo() {
        delete content_stream;
    }
};

struct CurrAnimation {
    std::unique_ptr<FileInfo, void (*)(FileInfo *)> file;
    rgb_matrix::StreamReader reader;
    const tmillis_t end_time_ms;

    CurrAnimation(std::unique_ptr<FileInfo, void (*)(FileInfo *)> file, const rgb_matrix::StreamReader &reader,
                  const tmillis_t end_time_ms) : file(std::move(file)),
                                                 reader(reader),
                                                 end_time_ms(
                                                         end_time_ms) {}
};

struct ImageInfo {
    vector<Magick::Image> frames;
    std::variant<std::unique_ptr<Post, void (*)(Post *)>, std::shared_ptr<Post>> post;
};

struct CurrPreset {
    ConfigData::Preset preset;
    uint curr_category;

    explicit CurrPreset(ConfigData::Preset preset) : preset(std::move(preset)), curr_category(0) {}
};

class ImageScene : public Scenes::Scene {
private:
    std::optional<CurrAnimation> curr_animation;
    std::optional<CurrPreset> curr_preset;
    optional<std::future<expected<optional<ImageInfo>, string>>> next_img;


    bool DisplayAnimation(rgb_matrix::RGBMatrix *matrix);

    expected<CurrAnimation, string>
    get_next_anim(rgb_matrix::RGBMatrix *matrix, int recursiveness);

    static expected<optional<ImageInfo>, string>
    get_next_image(std::shared_ptr<ImageProviders::General> category, int width, int height);

    static std::unique_ptr<FileInfo, void (*)(FileInfo *)>
    GetFileInfo(vector<Magick::Image> frames, FrameCanvas *canvas);

public:
    bool render(rgb_matrix::RGBMatrix *matrix) override;

    [[nodiscard]] string get_name() const override;

    void register_properties() override {}

    using Scenes::Scene::Scene;
    ~ImageScene() override = default;
};

class ImageSceneWrapper : public Plugins::SceneWrapper {
    std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)> create() override;
};