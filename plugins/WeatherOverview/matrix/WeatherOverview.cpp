#include <iostream>
#include "WeatherOverview.h"
#include "scenes/WeatherScene.h"
#include "scenes/WeatherAmbienceScene.h"
#include "shared/matrix/utils/shared.h"
#include "shared/matrix/server/server_utils.h"
#include "spdlog/spdlog.h"
#include "Constants.h"

using namespace Scenes;
using json = nlohmann::json;

REGISTER_PLUGIN(WeatherOverview, WeatherOverview)

vector<std::unique_ptr<ImageProviderWrapper> >
WeatherOverview::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> WeatherOverview::create_scenes() {
    vector<std::unique_ptr<SceneWrapper>> scenes;
    scenes.push_back(std::make_unique<WeatherSceneWrapper>());
    scenes.push_back(std::make_unique<WeatherAmbienceSceneWrapper>());
    return scenes;
}

std::unique_ptr<router_t> WeatherOverview::register_routes(std::unique_ptr<router_t> router) {
    router->http_post("/weather/indoor_temp", [](auto req, auto) {
        try {
            json j = json::parse(req->body());
            float value = j.value("temperature", -999.0f);
            indoor_temperature.store(value);
            spdlog::info("Indoor temperature updated: {:.1f}C", value);
            return Server::reply_with_json(req, {{"success", true}, {"temperature", value}});
        } catch (const std::exception &ex) {
            spdlog::warn("Invalid indoor temperature payload: {}", ex.what());
            return Server::reply_with_error(req, "Invalid JSON payload");
        }
    });
    return std::move(router);
}

std::optional<string> WeatherOverview::before_server_init() {
    auto conf = config->get_plugin_configs();

    const std::filesystem::path lib_path(get_plugin_location());
    const auto parent = lib_path.parent_path();

    auto plugin_weather_dir = parent / "fonts";
    if (std::getenv("WEATHER_FONT_DIRECTORY") != nullptr) {
        const std::string env_var = std::getenv("WEATHER_FONT_DIRECTORY");
        plugin_weather_dir = env_var;
    }

    spdlog::trace("Using fonts in {}", parent.string());
    const std::string HEADER_FONT_FILE = plugin_weather_dir.string() + "/7x13.bdf";
    const std::string BODY_FONT_FILE = plugin_weather_dir.string() + "/5x8.bdf";
    const std::string SMALL_FONT_FILE = plugin_weather_dir.string() + "/4x6.bdf";

    spdlog::debug("Loading fonts...");
    HEADER_FONT.emplace();
    BODY_FONT.emplace();
    SMALL_FONT.emplace();

    if (!HEADER_FONT->LoadFont(HEADER_FONT_FILE.c_str()))
        return "Could not load header font at " + HEADER_FONT_FILE;

    if (!BODY_FONT->LoadFont(BODY_FONT_FILE.c_str()))
        return "Could not load body font at " + BODY_FONT_FILE;

    if (!SMALL_FONT->LoadFont(SMALL_FONT_FILE.c_str()))
        return "Could not load small font at " + SMALL_FONT_FILE;

    return BasicPlugin::before_server_init();
}

std::optional<string> WeatherOverview::pre_exit() {
    HEADER_FONT.reset();
    BODY_FONT.reset();
    SMALL_FONT.reset();
    return BasicPlugin::pre_exit();
}
