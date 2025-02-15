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

void PluginManager::destroy_plugins() {
    info("Destroying plugins...");
    std::flush(std::cout);


    for (const auto &item: loaded_plugins) {
        void (*destroy)(BasicPlugin *);
        destroy = (void (*)(BasicPlugin *)) dlsym(item.handle, item.destroyFnName.c_str());

        destroy(item.plugin);
    }
}

void PluginManager::delete_references() {
    all_scenes.clear();

    for (auto &item: loaded_plugins) {
        item.sceneWrappers.clear();
        item.imageProviderWrappers.clear();
    }
}


std::vector<BasicPlugin *> PluginManager::get_plugins() {
    std::vector<BasicPlugin *> plugins;
    for (const auto &item: loaded_plugins) {
        plugins.emplace_back(item.plugin);
    }

    return plugins;
}

std::vector<std::shared_ptr<SceneWrapper>> &PluginManager::get_scenes() {
    if (all_scenes.size() == 0) {
        for (const auto &item: get_plugins()) {
            auto pl_scenes = item->get_scenes();
            all_scenes.insert(all_scenes.end(),
                              pl_scenes.begin(),
                              pl_scenes.end());
        }
    }

    return all_scenes;
}

std::vector<std::shared_ptr<Plugins::ImageProviderWrapper> > PluginManager::get_image_providers() {
    std::vector<std::shared_ptr<Plugins::ImageProviderWrapper> > types;
    for (const auto &item: get_plugins()) {
        auto pl_providers = item->get_image_providers();
        types.insert(types.end(),
                     pl_providers.begin(),
                     pl_providers.end()
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
    for (const std::string &pl_name: libNames) {
        // Clear any existing errors
        dlerror();

        void *dlhandle = dlopen(pl_name.c_str(), RTLD_LAZY);
        if (dlhandle == nullptr) {
            error("Failed to load plugin '{}': {}", pl_name, dlerror());
            continue; // Skip this plugin and try the next one
        }

        std::string pl_copy = pl_name;
        std::pair<std::string, std::string> delibbed = Plugins::get_lib_name(pl_copy);
        std::string cn = "create" + delibbed.second;
        std::string dn = "destroy" + delibbed.second;

        // Clear any existing errors before dlsym
        dlerror();

        BasicPlugin *(*create)() = (BasicPlugin *(*)()) (dlsym(dlhandle, cn.c_str()));
        const char *dlsym_error = dlerror();

        if (dlsym_error != nullptr) {
            error("Symbol lookup error in plugin '{}': {}", pl_name, dlsym_error);
            error("Expected symbol '{}' not found", cn);
            dlclose(dlhandle);
            continue; // Skip this plugin and try the next one
        }

        // Verify destroy function exists before creating plugin
        dlerror();
        void *destroy_sym = dlsym(dlhandle, dn.c_str());
        if (dlerror() != nullptr || destroy_sym == nullptr) {
            error("Destroy function '{}' not found in plugin '{}'", dn, pl_name);
            dlclose(dlhandle);
            continue;
        }

        try {
            BasicPlugin *p = create();
            Dl_info dl_info;
            dladdr((void *) create, &dl_info);

            p->_plugin_location = dl_info.dli_fname;
            info("Successfully loaded plugin {}", pl_name);

            PluginInfo info = {
                .handle = dlhandle,
                .destroyFnName = dn,
                .plugin = p,
            };
            loaded_plugins.emplace_back(info);
        } catch (const std::exception &e) {
            error("Failed to initialize plugin '{}': {}", pl_name, e.what());
            dlclose(dlhandle);
        }
    }

    info("Loaded a total of {} plugins.", loaded_plugins.size());
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
