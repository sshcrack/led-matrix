#include <iostream>
#include "WeatherOverview.h"
#include "scenes/WeatherScene.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
#include "Constants.h"

using namespace Scenes;

extern "C" [[maybe_unused]] WeatherOverview *createWeatherOverview() {
    return new WeatherOverview();
}

extern "C" [[maybe_unused]] void destroyWeatherOverview(WeatherOverview *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)> >
WeatherOverview::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)> > WeatherOverview::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)> >();
    
    // Add the original weather scene
    scenes.push_back({
        new WeatherSceneWrapper(), [](SceneWrapper *scene) {
            delete scene;
        }
    });

    return scenes;
}

std::optional<string> WeatherOverview::before_server_init() {
    auto conf = config->get_plugin_configs();

    const std::filesystem::path lib_path(get_plugin_location());
    const auto parent = lib_path.parent_path();

    auto plugin_weather_dir = parent / "weather-overview/fonts";
    if (std::getenv("WEATHER_FONT_DIRECTORY") != nullptr) {
        const std::string env_var = std::getenv("WEATHER_FONT_DIRECTORY");;
        plugin_weather_dir = env_var;
    }

    spdlog::trace("Using fonts in {}", parent.c_str());
    const std::string HEADER_FONT_FILE = std::string(plugin_weather_dir) + "/7x13.bdf";
    const std::string BODY_FONT_FILE = std::string(plugin_weather_dir) + "/5x8.bdf";
    const std::string SMALL_FONT_FILE = std::string(plugin_weather_dir) + "/4x6.bdf";

    spdlog::debug("Loading font...");
    const auto headerRes = HEADER_FONT.LoadFont(HEADER_FONT_FILE.c_str());
    const auto bodyRes = BODY_FONT.LoadFont(BODY_FONT_FILE.c_str());
    const auto smallRes = SMALL_FONT.LoadFont(SMALL_FONT_FILE.c_str());

    if (!headerRes)
        return "Could not load header font at " + HEADER_FONT_FILE;

    if (!bodyRes)
        return "Could not load body font at " + BODY_FONT_FILE;

    if(!smallRes)
        return "Could not load small font at " + SMALL_FONT_FILE;

    return BasicPlugin::before_server_init();
}
