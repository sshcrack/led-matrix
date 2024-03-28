#pragma once

#include "led-matrix.h"
#include <optional>
#include <utility>
#include <future>
#include "Scene.h"
#include "../../utils/utils.h"
#include "content-streamer.h"
#include "../../config/data.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;


static const tmillis_t distant_future = (1LL << 40); // that is a while.
struct ImageParams {
    ImageParams() : anim_duration_ms(distant_future), wait_ms(1500),
                    anim_delay_ms(-1), vsync_multiple(1) {}

    tmillis_t anim_duration_ms;  // If this is an animation, duration to show.
    tmillis_t wait_ms;           // Regular image: duration to show.
    tmillis_t anim_delay_ms;     // Animation delay override.
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
    const tmillis_t override_anim_delay;

    CurrAnimation(FileInfo file, rgb_matrix::StreamReader reader, const tmillis_t end_time_ms, const tmillis_t override_anim_delay) : file(file), reader(reader), end_time_ms(end_time_ms), override_anim_delay(override_anim_delay) {}
};

using ImageInfo = tuple<vector<Magick::Image>, Post>;
struct CurrPreset {
    ConfigData::Preset preset;
    uint curr_category;

    explicit CurrPreset(ConfigData::Preset preset): preset(std::move(preset)), curr_category(0) {}
};

class PresetScene : public Scene {
private:
    std::optional<CurrAnimation> curr_animation;
    std::optional<CurrPreset> curr_preset;
    optional<std::future<expected<optional<ImageInfo>, string>>> next_img;
    void DisplayAnimation(RGBMatrix *matrix);

    expected<CurrAnimation, string> get_next_anim(RGBMatrix *matrix, int recursiveness);
public:
    void tick(RGBMatrix *matrix) override;
};