#include <dlfcn.h>

#include <set>
#include <filesystem>
#include "spdlog/spdlog.h"

#include "shared/matrix/plugin_loader/loader.h"
#include "shared/common/plugin_loader/lib_name.h"
#include "shared/matrix/plugin/main.h"

using namespace spdlog;
namespace fs = std::filesystem;
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
        for (auto &item: get_plugins()) {
            auto pl_scenes = item->get_scenes();
            all_scenes.insert(all_scenes.end(),
                              pl_scenes.begin(),
                              pl_scenes.end());
        }
    }

    return all_scenes;
}

std::vector<std::shared_ptr<ImageProviderWrapper> > PluginManager::get_image_providers() {
    std::vector<std::shared_ptr<ImageProviderWrapper> > types;
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
    auto raw_plugin = getenv("PLUGIN_DIR");

    fs::path plugin_dir = exec_dir / "plugins";
    if(raw_plugin != nullptr) {
        plugin_dir = std::filesystem::path(raw_plugin);
    }

    std::vector<fs::path> libPaths;
    for (const auto &entry : fs::directory_iterator(plugin_dir))
    {
        if (!entry.is_directory())
            continue;
        fs::path plugin_dir_path = entry.path();
        fs::path plugin_path = plugin_dir_path / (std::string("lib") += plugin_dir_path.filename() += ".so");
        if (!fs::is_regular_file(plugin_path))
            continue;

        libPaths.push_back(fs::absolute(plugin_path));
    }

    // Loading libs to memory
    for (const fs::path &plPath: libPaths) {
        // Clear any existing errors
        dlerror();

        void *dlhandle = dlopen(plPath.c_str(), RTLD_LAZY);
        if (dlhandle == nullptr) {
            error("Failed to load plugin '{}': {}", plPath.string(), dlerror());
            continue; // Skip this plugin and try the next one
        }

        std::string libName = Plugins::get_lib_name(plPath);

        std::string cn = "create" + libName;
        std::string dn = "destroy" + libName;

        // Clear any existing errors before dlsym
        dlerror();

        BasicPlugin *(*create)() = (BasicPlugin *(*)()) (dlsym(dlhandle, cn.c_str()));
        const char *dlsym_error = dlerror();

        if (dlsym_error != nullptr) {
            error("Symbol lookup error in plugin '{}': {}", plPath.string(), dlsym_error);
            error("Expected symbol '{}' not found", cn);
            dlclose(dlhandle);
            continue; // Skip this plugin and try the next one
        }

        // Verify destroy function exists before creating plugin
        dlerror();
        void *destroy_sym = dlsym(dlhandle, dn.c_str());
        if (dlerror() != nullptr || destroy_sym == nullptr) {
            error("Destroy function '{}' not found in plugin '{}'", dn, plPath.string());
            dlclose(dlhandle);
            continue;
        }

        try {
            BasicPlugin *p = create();
            Dl_info dl_info;
            dladdr((void *) create, &dl_info);

            p->_plugin_location = dl_info.dli_fname;
            info("Successfully loaded plugin {}", plPath.string());

            PluginInfo info = {
                .handle = dlhandle,
                .destroyFnName = dn,
                .plugin = p,
            };
            loaded_plugins.emplace_back(info);
        } catch (const std::exception &e) {
            error("Failed to initialize plugin '{}': {}", plPath.string(), e.what());
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
