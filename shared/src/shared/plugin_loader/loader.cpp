#include <dlfcn.h>

#include <set>
#include <spdlog/spdlog.h>

#include "shared/plugin_loader/loader.h"
#include "lib_name.h"
#include "lib_glob.h"
#include "plugin/main.h"

using namespace spdlog;
using Plugins::BasicPlugin;
using Plugins::ImageProviderWrapper;
using Plugins::PluginManager;
using Plugins::SceneWrapper;

PluginManager::PluginManager() = default;

void PluginManager::terminate() {
    info("Destroying plugins...");
    std::flush(std::cout);

    for (const auto &item: loaded_plugins) {
        void (*destroy)(BasicPlugin *);
        destroy = (void (*)(BasicPlugin *)) dlsym(get < 0 > (item), get < 1 > (item).c_str());

        destroy(get < 2 > (item));
    }
}

std::vector<Plugins::BasicPlugin*> PluginManager::get_plugins() {
    std::vector<Plugins::BasicPlugin*> plugins;
    for (const auto &item: loaded_plugins) {
        plugins.emplace_back(get < 2 > (item));
    }

    return plugins;
}

std::vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> PluginManager::get_scenes() {
    std::vector<std::unique_ptr<SceneWrapper, void(*)(SceneWrapper*)>> scenes;

    for (const auto &item: get_plugins()) {
        auto pl_scenes = item->get_scenes();
        scenes.insert(scenes.end(),
                      std::make_move_iterator(pl_scenes.begin()),
                      std::make_move_iterator(pl_scenes.end()));
    }

    return scenes;
}

std::vector<std::unique_ptr<Plugins::ImageProviderWrapper>> PluginManager::get_image_providers() {
    std::vector<std::unique_ptr<Plugins::ImageProviderWrapper>> types;
    for (const auto &item: get_plugins()) {
        auto pl_providers = item->get_image_providers();
        types.insert(types.end(),
                     std::make_move_iterator(pl_providers.begin()),
                     std::make_move_iterator(pl_providers.end())
        );
    }

    return types;
}

void PluginManager::initialize() {
    if (initialized)
        return;

    auto exec_dir = get_exec_dir();
    if (!exec_dir)
        throw std::runtime_error("Could not get executable directory");

    const char *plugin_dir = getenv("PLUGIN_DIR");

    const std::string libGlob(plugin_dir == nullptr ? "plugins/*.so" : std::string(plugin_dir) + "/*.so");

    std::vector<std::string> filenames = Plugins::lib_glob(exec_dir.value() + "/" + libGlob);

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

        create = (BasicPlugin *(*)()) dlsym(dlhandle, cn.c_str());
        if (create == nullptr) {
            error("Could not find symbol '{}' for Plugin '{}'. Output: {}", cn, pl_name, dlerror());
            std::exit(-1);
        }

        BasicPlugin *p = create();

        info("Loaded plugin {}", pl_name);
        loaded_plugins.emplace_back(dlhandle, dn, p);
    }

    info("Loaded a total of {} plugins.", libNames.size());
    info("Loading providers to register...");

    initialized = true;
}

PluginManager *PluginManager::instance_ = nullptr;

PluginManager *PluginManager::instance() {
    if (instance_ == nullptr) {
        instance_ = new PluginManager();
    }

    return instance_;
}