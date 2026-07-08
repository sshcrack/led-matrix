#include "shared/matrix/plugin_loader/loader.h"

using namespace spdlog;
using Plugins::BasicPlugin;
using Plugins::ImageProviderWrapper;
using Plugins::PluginManager;
using Plugins::SceneWrapper;

PluginManager::PluginManager()
    : PluginLoader("led-matrix", RTLD_LAZY | RTLD_GLOBAL)
{}

PluginManager *PluginManager::instance_ = nullptr;

PluginManager *PluginManager::instance() {
    if (instance_ == nullptr) {
        instance_ = new PluginManager();
    }
    return instance_;
}

std::vector<BasicPlugin *> PluginManager::get_plugins() {
    std::vector<BasicPlugin *> plugins;
    for (const auto &item : loaded_plugins) {
        plugins.emplace_back(item.plugin);
    }
    return plugins;
}

std::vector<std::shared_ptr<SceneWrapper>> PluginManager::get_scenes() {
    std::lock_guard<std::mutex> lock(scenes_mutex);
    if (!scenes_initialized) {
        for (auto &item : get_plugins()) {
            auto pl_scenes = item->get_scenes();
            all_scenes.insert(all_scenes.end(),
                              pl_scenes.begin(),
                              pl_scenes.end());
        }
        scenes_initialized = true;
    }
    return all_scenes;
}

void PluginManager::add_scene(std::shared_ptr<SceneWrapper> scene) {
    std::lock_guard<std::mutex> lock(scenes_mutex);
    all_scenes.push_back(std::move(scene));
}

void PluginManager::remove_scene(const std::string& name) {
    std::lock_guard<std::mutex> lock(scenes_mutex);
    all_scenes.erase(
        std::remove_if(all_scenes.begin(), all_scenes.end(),
                       [&name](const std::shared_ptr<SceneWrapper>& s) {
                           return s->get_name() == name;
                       }),
        all_scenes.end()
    );
}

std::vector<std::shared_ptr<ImageProviderWrapper>> PluginManager::get_image_providers() {
    std::vector<std::shared_ptr<ImageProviderWrapper>> types;
    for (const auto &item : get_plugins()) {
        auto pl_providers = item->get_image_providers();
        types.insert(types.end(),
                     pl_providers.begin(),
                     pl_providers.end());
    }
    return types;
}

std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> PluginManager::get_shader_providers() {
    std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> types;
    for (const auto &item : get_plugins()) {
        auto pl_providers = item->get_shader_providers();
        types.insert(types.end(),
                     pl_providers.begin(),
                     pl_providers.end());
    }
    return types;
}

void PluginManager::delete_references() {
    all_scenes.clear();
    PluginLoader::delete_references();
}
