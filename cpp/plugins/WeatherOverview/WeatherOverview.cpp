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
    auto conf = config->get_plugin_configs();
    if(conf.find("weatherApiKey") == conf.end()) {
        throw std::runtime_error("Config value 'pluginConfigs.weatherApiKey' is not set. Set it to an OpenWeatherMap API key.");
    }

    LOCATION_LAT = conf["weatherLat"];
    LOCATION_LON = conf["weatherLon"];
}

vector<SceneWrapper *> WeatherOverview::get_scenes() {
    check_config();

    return {
            new WeatherSceneWrapper(),
    };
}

WeatherOverview::WeatherOverview() = default;
