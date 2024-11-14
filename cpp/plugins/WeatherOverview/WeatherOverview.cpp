#include <iostream>
#include "WeatherOverview.h"
#include "scenes/WeatherScene.h"
#include "shared/utils/shared.h"
#include <spdlog/spdlog.h>
#include "Constants.h"

using namespace Scenes;

extern "C" [[maybe_unused]] WeatherOverview *createWeatherOverview() {
    return new WeatherOverview();
}

extern "C" [[maybe_unused]] void destroyWeatherOverview(WeatherOverview *c) {
    delete c;
}

vector<ImageProviderWrapper *> WeatherOverview::get_image_providers() {
    return {};
}

vector<SceneWrapper *> WeatherOverview::get_scenes() {
    return {
            new WeatherSceneWrapper(),
    };
}

std::optional<string> WeatherOverview::post_init() {
    auto conf = config->get_plugin_configs();
    if (conf.find("weatherLat") == conf.end()) {
        std::cout << "throwing error" << std::endl << std::flush;
        return "Config value 'pluginConfigs.weatherLat' is not set."
               "Set it to the latitude of the city you want to display the weather for.";
    }

    if (conf.find("weatherLon") == conf.end()) {
        return
                "Config value 'pluginConfigs.weatherLon' is not set."
                "Set it to the longitude of the city you want to display the weather for.";
    }

    LOCATION_LAT = conf["weatherLat"];
    LOCATION_LON = conf["weatherLon"];


    spdlog::debug("Loading font...");
    auto headerRes = HEADER_FONT.LoadFont(HEADER_FONT_FILE.c_str());
    auto bodyRes = BODY_FONT.LoadFont(BODY_FONT_FILE.c_str());

    if (!headerRes)
        return "Could not load header font at " + HEADER_FONT_FILE;

    if (!bodyRes)
        return "Could not load body font at " + BODY_FONT_FILE;

    return nullopt;
}

WeatherOverview::WeatherOverview() = default;
