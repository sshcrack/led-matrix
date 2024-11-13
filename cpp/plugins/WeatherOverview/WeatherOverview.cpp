#include <iostream>
#include "WeatherOverview.h"
#include "scenes/WeatherScene.h"
#include "shared/utils/shared.h"
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

void check_config() {
    std::cout << "Checking config" << std::endl << std::flush;

    auto conf = config->get_plugin_configs();
    if (conf.find("weatherLat") == conf.end()) {
        std::cout << "throwing error" << std::endl << std::flush;
        throw std::runtime_error(
                "Config value 'pluginConfigs.weatherLat' is not set. Set it to the latitude of the city you want to display the weather for.");
    }

    if (conf.find("weatherLon") == conf.end()) {
        throw std::runtime_error(
                "Config value 'pluginConfigs.weatherLon' is not set. Set it to the longitude of the city you want to display the weather for.");
    }

    LOCATION_LAT = conf["weatherLat"];
    LOCATION_LON = conf["weatherLon"];
}

vector<SceneWrapper *> WeatherOverview::get_scenes() {
    check_config();
    if (!hasLoadedFonts) {
        hasLoadedFonts = true;
        auto headerRes = HEADER_FONT.LoadFont(HEADER_FONT_FILE.c_str());
        auto bodyRes = BODY_FONT.LoadFont(BODY_FONT_FILE.c_str());

        if (!headerRes)
            throw std::runtime_error("Could not load header font at " + HEADER_FONT_FILE);

        if (!bodyRes)
            throw std::runtime_error("Could not load body font at " + BODY_FONT_FILE);
    }

    return {
            new WeatherSceneWrapper(),
    };
}

WeatherOverview::WeatherOverview() = default;
