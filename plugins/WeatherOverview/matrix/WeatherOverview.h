#pragma once

#include "shared/matrix/plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::BasicPlugin;

class WeatherOverview final : public BasicPlugin {
public:
    using BasicPlugin::BasicPlugin;

    vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> create_scenes() override;
    vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>> create_image_providers() override;
    std::optional<string> before_server_init() override;
};

extern "C" PLUGIN_EXPORT WeatherOverview *createWeatherOverview();
extern "C" PLUGIN_EXPORT void destroyWeatherOverview(WeatherOverview *c);