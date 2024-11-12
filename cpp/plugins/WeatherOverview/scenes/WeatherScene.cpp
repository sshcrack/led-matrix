#include "WeatherScene.h"
#include "../Constants.h"

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
    /**
     * export enum SkyColor {
	DAY_CLEAR = "#3aa1d5",
	DAY_CLOUDS = "#7c8c9a",
	NIGHT_CLEAR = "#262b49",
	NIGHT_CLOUDS = "#2d2e34"
}
     */
    offscreen_canvas->Fill();

    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    return false;
}


std::expected<std::string, std::string> fetch_api() {
    //TODO
    return "";
}

struct WeatherData {
    int color;
};

std::expected<WeatherData, string> parse_weather_data(const std::string &str_data) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(str_data);


        auto curr = json.at("current");
        bool night = curr["is_day"].get<int>() == 0;
        bool clouds = curr["cloud_cover"].get<int>() > 50;

        int color = SkyColor::DAY_CLEAR;

        if (clouds) color = SkyColor::DAY_CLOUDS;
        if (night) color = SkyColor::NIGHT_CLEAR;
        if (clouds && night) color = SkyColor::NIGHT_CLOUDS;


        auto data = WeatherData();
        data.color = color;

        return data;
    } catch (std::exception &ex) {
        return std::unexpected("Could not parse json: " + std::string(ex.what()));
    }
}