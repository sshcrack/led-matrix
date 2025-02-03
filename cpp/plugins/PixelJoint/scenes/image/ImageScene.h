#pragma once

#include "led-matrix.h"
#include <optional>
#include <utility>
#include <future>
#include "Scene.h"
#include "plugin.h"
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
};

struct CurrAnimation {
    FileInfo file;
    rgb_matrix::StreamReader reader;
    const tmillis_t end_time_ms;

    CurrAnimation(FileInfo file, const rgb_matrix::StreamReader& reader, const tmillis_t end_time_ms) : file(file), reader(reader), end_time_ms(end_time_ms)
    {}
};

using ImageInfo = tuple<vector<Magick::Image>, Post>;

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

    expected<CurrAnimation, string> get_next_anim(rgb_matrix::RGBMatrix *matrix, int recursiveness);

    static expected<optional<ImageInfo>, string>
    get_next_image(ImageProviders::General *category, int width, int height);

    static FileInfo GetFileInfo(tuple<vector<Magick::Image>, Post> p_info, FrameCanvas *canvas);

public:
    bool render(rgb_matrix::RGBMatrix *matrix) override;
    [[nodiscard]] string get_name() const override;

    using Scenes::Scene::Scene;
};

class ImageSceneWrapper : public Plugins::SceneWrapper {
    Scenes::Scene * create_default() override;
    Scenes::Scene * from_json(const nlohmann::json &args) override;
};