#include <spdlog/spdlog.h>
#include "WeatherScene.h"
#include "../Constants.h"
#include "../icons/weather_icons.h"
#include "shared/utils/consts.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"

string root_dir = Constants::root_dir;

int main_icon_size = 50;
struct Images {
    std::vector<uint8_t> currentIcon;
};

std::optional<Images> images;

Scenes::Scene *Scenes::WeatherSceneWrapper::create_default() {
    return new WeatherScene(Scene::create_default(3, 20 * 1000));
}

Scenes::Scene *Scenes::WeatherSceneWrapper::from_json(const json &args) {
    return new WeatherScene(args);
}

string Scenes::WeatherScene::get_name() const {
    return "weather";
}

bool should_render = true;

bool Scenes::WeatherScene::render(RGBMatrix *matrix) {
    auto data_res = parser->get_data();
    if (!data_res) {
        spdlog::warn("Could not get weather data_res: {}", data_res.error());
        return false;
    }

    auto data = data_res.value();
    auto should_calc_images = parser->has_changed() || !images.has_value();
    if (should_calc_images && !data.icon_url.empty()) {
        string file_path = std::filesystem::path(root_dir + "weather_icon" + std::to_string(data.weatherCode) + ".png");
        std::filesystem::path processed_img = to_processed_path(file_path);

        // Downloading image first
        if (!filesystem::exists(processed_img)) {
            try_remove(file_path);
            auto res = download_image(data.icon_url, file_path);
            if (!res) {
                spdlog::warn("Could not download image");
                return false;
            }
        }

        bool contain_img = true;
        auto res = LoadImageAndScale(file_path, main_icon_size, main_icon_size, true, true, contain_img);
        if (!res) {
            spdlog::error("Error loading image: {}", res.error());
            try_remove(file_path);

            return false;
        }

        vector<Magick::Image> frames = std::move(res.value());
        try_remove(file_path);
        parser->unmark_changed();

        Images img;
        img.currentIcon = magick_to_rgb(frames.at(0));

        images = img;
        should_render = true;
    }

    /**
     * export enum SkyColor {
	DAY_CLEAR = "#3aa1d5",
	DAY_CLOUDS = "#7c8c9a",
	NIGHT_CLEAR = "#262b49",
	NIGHT_CLOUDS = "#2d2e34"
}
     */
    if (!should_render)
        return false;

    offscreen_canvas->Clear();
    offscreen_canvas->Fill(data.color.r, data.color.g, data.color.b);

    // Images must be a value here
    if (images.has_value()) {
        SetImageTransparent(offscreen_canvas, 0, 0,
                            images->currentIcon.data(), images->currentIcon.size(),
                            main_icon_size, main_icon_size, 0, 0, 0);
    }


    auto header_y = HEADER_FONT.height() / 2 + main_icon_size / 2;
    DrawText(offscreen_canvas, HEADER_FONT, 60, header_y,
             {255, 255, 255}, data.temperature.c_str());

    spdlog::trace("Header height: {} with baseline {} with {}", header_y, HEADER_FONT.baseline(), data.temperature);
    DrawText(offscreen_canvas, BODY_FONT, 60, header_y + 20,
             {255, 255, 255}, data.description.c_str());

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    should_render = false;
    return true;
}

void Scenes::WeatherScene::cleanup(rgb_matrix::RGBMatrix *matrix) {
    should_render = true;
    Scene::cleanup(matrix);
}
