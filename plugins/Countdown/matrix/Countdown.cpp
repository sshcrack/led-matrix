#include "Countdown.h"
#include "scenes/CountdownScene.h"
#include "Constants.h"
#include <spdlog/spdlog.h>

using namespace std;

REGISTER_PLUGIN(Countdown, Countdown)

vector<std::unique_ptr<ImageProviderWrapper>> Countdown::create_image_providers() {
    return {};
}

vector<std::unique_ptr<SceneWrapper>> Countdown::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper>>();

    scenes.push_back(std::make_unique<Scenes::CountdownSceneWrapper>());
    return scenes;
}

Countdown::Countdown() = default;

std::optional<string> Countdown::before_server_init() {
    // Determine font directory (allow override via env)
    auto plugin_dir = std::filesystem::path(get_plugin_location()).parent_path();
    auto font_dir = plugin_dir / "fonts";
    if (std::getenv("COUNTDOWN_FONT_DIRECTORY") != nullptr) {
        font_dir = std::getenv("COUNTDOWN_FONT_DIRECTORY");
    }

    const std::string HEADER_FONT_FILE = std::string(font_dir) + "/7x13.bdf";
    const std::string BODY_FONT_FILE = std::string(font_dir) + "/5x8.bdf";
    const std::string SMALL_FONT_FILE = std::string(font_dir) + "/4x6.bdf";

    spdlog::debug("Loading Countdown fonts from {}", font_dir.string());

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

std::optional<string> Countdown::pre_exit() {
    HEADER_FONT.reset();
    BODY_FONT.reset();
    SMALL_FONT.reset();
    return BasicPlugin::pre_exit();
}
