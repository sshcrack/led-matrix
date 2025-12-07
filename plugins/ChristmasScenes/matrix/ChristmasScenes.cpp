#include "ChristmasScenes.h"
#include "Constants.h"
#include "scenes/SnowfallScene.h"
#include "scenes/ChristmasTreeScene.h"
#include "scenes/FireplaceScene.h"
#include "scenes/ChristmasCountdownScene.h"
#include <filesystem>

// CRITICAL: Function names must match pattern create<PluginName> and destroy<PluginName>
extern "C" PLUGIN_EXPORT ChristmasScenes *createChristmasScenes() {
    return new ChristmasScenes();
}

extern "C" PLUGIN_EXPORT void destroyChristmasScenes(ChristmasScenes *c) {
    delete c;
}

vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>> 
ChristmasScenes::create_image_providers() {
    return {}; // No image providers for this plugin
}

vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)>> 
ChristmasScenes::create_scenes() {
    auto scenes = vector<std::unique_ptr<SceneWrapper, void(*)(Plugins::SceneWrapper*)>>();
    auto deleteScene = [](SceneWrapper* scene) { delete scene; };

    scenes.push_back({new Scenes::SnowfallSceneWrapper(), deleteScene});
    scenes.push_back({new Scenes::ChristmasTreeSceneWrapper(), deleteScene});
    scenes.push_back({new Scenes::FireplaceSceneWrapper(), deleteScene});
    scenes.push_back({new Scenes::ChristmasCountdownSceneWrapper(), deleteScene});
    
    return scenes;
}

std::optional<string> ChristmasScenes::before_server_init() {
    auto plugin_dir = std::filesystem::path(get_plugin_location()).parent_path();
    auto font_dir = plugin_dir / "fonts";
    
    const std::string HEADER_FONT_FILE = std::string(font_dir) + "/7x13.bdf";
    const std::string BODY_FONT_FILE = std::string(font_dir) + "/5x8.bdf";
    const std::string SMALL_FONT_FILE = std::string(font_dir) + "/4x6.bdf";
    
    if (!HEADER_FONT.LoadFont(HEADER_FONT_FILE.c_str()))
        return "Could not load header font at " + HEADER_FONT_FILE;
    
    if (!BODY_FONT.LoadFont(BODY_FONT_FILE.c_str()))
        return "Could not load body font at " + BODY_FONT_FILE;
        
    if (!SMALL_FONT.LoadFont(SMALL_FONT_FILE.c_str()))
        return "Could not load small font at " + SMALL_FONT_FILE;
    
    return BasicPlugin::before_server_init();
}

ChristmasScenes::ChristmasScenes() = default;
