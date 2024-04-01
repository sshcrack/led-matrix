#include <dlfcn.h>

#include <set>
#include <spdlog/spdlog.h>

#include "shared/plugin_loader/loader.h"
#include "lib_name.h"
#include "lib_glob.h"
#include "plugin.h"

using namespace spdlog;
using Plugins::BasicPlugin;
using Plugins::PluginManager;
using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;

PluginManager::PluginManager() = default;

void PluginManager::terminate() {
    info("Destroying plugins...");
    for (const auto &item: loaded_plugins) {
        void (*destroy)(BasicPlugin *);
        destroy = (void (*)(BasicPlugin *)) dlsym(get < 0 > (item), get < 1 > (item).c_str());

        destroy(get < 2 > (item));
    }
}


std::vector<BasicPlugin *> PluginManager::get_plugins() {
    std::vector<BasicPlugin *> plugins;
    for (const auto &item: loaded_plugins) {
        plugins.emplace_back(get < 2 > (item));
    }

    return plugins;
}


std::vector<SceneWrapper *> PluginManager::get_scenes() {
    std::vector<SceneWrapper *> scenes;

    for (const auto &item: get_plugins()) {
        auto pl_scenes = item->get_scenes();
        scenes.insert(scenes.end(), pl_scenes.begin(), pl_scenes.end());
    }

    return scenes;
}


std::vector<ImageProviderWrapper *> PluginManager::get_image_providers() {
    std::vector<ImageProviderWrapper *> types;
    for (const auto &item: get_plugins()) {
        auto pl_providers = item->get_image_providers();
        types.insert(types.end(), pl_providers.begin(), pl_providers.end());
    }

    return types;
}

void PluginManager::initialize() {
    if (initialized)
        return;

    const std::string libGlob("plugins/*.so");

    std::vector<std::string> filenames = Plugins::lib_glob(libGlob);

    std::set<std::string> libNames;
    for (const std::string &p_name: filenames) {
        libNames.insert(p_name);
    }


    // Loading libs to memory

    for (std::string pl_name: libNames) {
        void *dlhandle = dlopen(pl_name.c_str(), RTLD_LAZY);

        std::pair<std::string, std::string> delibbed =
                Plugins::get_lib_name(pl_name);

        BasicPlugin *(*create)();

        std::string cn = "create" + delibbed.second;
        std::string dn = "destroy" + delibbed.second;

        flush(std::cout);
        create = (BasicPlugin *(*)()) dlsym(dlhandle, cn.c_str());
        if (create == nullptr) {
            error("Could not find symbol '{}' for Plugin '{}'. Output: {}", cn, pl_name, dlerror());
            std::exit(-1);
        }

        BasicPlugin *p = create();

        info("Loaded plugin {}", pl_name);
        loaded_plugins.emplace_back(dlhandle, dn, p);
    }

    info("Loaded plugins.");
    for (const auto &item: get_image_providers()) {
        auto defaultClass = item->create_default();

        img_provider_map[typeid(defaultClass).name()] = item->get_name();
    }


    for (const auto &item: get_scenes()) {
        auto defaultClass = item->create_default();

        scene_provider_map[typeid(defaultClass).name()] = item->get_name();
    }
    initialized = true;
}

PluginManager *PluginManager::instance_ = nullptr;

PluginManager *PluginManager::instance() {
    if (instance_ == nullptr) {
        instance_ = new PluginManager();
    }

    return instance_;
}

std::optional<string> Plugins::PluginManager::get_name_of_provider(ImageProviders::General *provider) {
    return img_provider_map[typeid(provider).name()];
}

std::optional<string> Plugins::PluginManager::get_name_of_scene(Scenes::Scene *provider) {
    return scene_provider_map[typeid(provider).name()];
}