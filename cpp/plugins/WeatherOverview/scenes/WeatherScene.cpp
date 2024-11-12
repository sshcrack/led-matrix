#include <spdlog/spdlog.h>
#include "WeatherScene.h"
#include "../Constants.h"
#include "../icons/weather_icons.h"
#include "shared/utils/consts.h"
#include "shared/utils/canvas_image.h"
#include "shared/utils/image_fetch.h"

string root_dir = Constants::root_dir;

Scenes::Scene *Scenes::WeatherSceneWrapper::create_default() {
    return new WeatherScene(Scene::create_default(3, 20 * 1000));
}

Scenes::Scene *Scenes::WeatherSceneWrapper::from_json(const json &args) {
    return new WeatherScene(args);
}

string Scenes::WeatherScene::get_name() const {
    return "weather";
}

bool Scenes::WeatherScene::tick(RGBMatrix *matrix) {
    auto data_res = parser->get_data();
    if (!data_res) {
        spdlog::warn("Could not get weather data_res: {}", data_res.error());
        return true;
    }

    auto data = data_res.value();
    string file_path = std::filesystem::path(root_dir + "weather_icon" + std::to_string(data.weatherCode) + ".png");
    std::filesystem::path processed_img = to_processed_path(file_path);

    // Downloading image first
    if (!filesystem::exists(processed_img)) {
        try_remove(file_path);
        auto res = download_image(data.icon_url, file_path);
        if (!res) {
            spdlog::warn("Could not download image");
            return true;
        }
    }

    bool contain_img = true;
    auto res = LoadImageAndScale(file_path, 50, 50, true, true, contain_img);
    if (!res) {
        spdlog::error("Error loading image: {}", res.error());
        try_remove(file_path);

        return true;
    }

    vector<Magick::Image> frames = std::move(res.value());
    try_remove(file_path);


    /**
     * export enum SkyColor {
	DAY_CLEAR = "#3aa1d5",
	DAY_CLOUDS = "#7c8c9a",
	NIGHT_CLEAR = "#262b49",
	NIGHT_CLOUDS = "#2d2e34"
}
     */
    offscreen_canvas->Fill(data.color.r, data.color.g, data.color.b);
    DrawText(offscreen_canvas, BODY_FONT, 0, HEADER_FONT.height(), {255, 255, 255}, data.description.c_str());


    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    return false;
}