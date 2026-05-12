#include <dlfcn.h>

#include <set>
#include <filesystem>
#include <algorithm>
#include <cstdint>
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

        if (destroy != nullptr) {
            destroy(item.plugin);
        } else {
            warn("Destroy function '{}' missing for plugin handle {}", item.destroyFnName, reinterpret_cast<uintptr_t>(item.handle));
        }

        if (item.handle != nullptr) {
            if (dlclose(item.handle) != 0) {
                warn("Failed to close plugin handle: {}", dlerror());
            }
        }
    }
    loaded_plugins.clear();
    initialized = false;
}

void PluginManager::delete_references() {
    all_scenes.clear();

    for (auto &item: loaded_plugins) {
        item.sceneWrappers.clear();
        item.imageProviderWrappers.clear();
        item.shaderProviderWrappers.clear();
    }
}


std::vector<BasicPlugin *> PluginManager::get_plugins() {
    std::vector<BasicPlugin *> plugins;
    for (const auto &item: loaded_plugins) {
        plugins.emplace_back(item.plugin);
    }

    return plugins;
}

std::vector<std::shared_ptr<SceneWrapper>> PluginManager::get_scenes() {
    {
        std::shared_lock<std::shared_mutex> read_lock(scenes_mutex);
        if (scenes_initialized) {
            return all_scenes;
        }
    }

    std::unique_lock<std::shared_mutex> write_lock(scenes_mutex);
    if (!scenes_initialized) {
        for (auto &item: get_plugins()) {
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
    std::unique_lock<std::shared_mutex> lock(scenes_mutex);
    all_scenes.push_back(std::move(scene));
}

void PluginManager::remove_scene(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(scenes_mutex);
    all_scenes.erase(
        std::remove_if(all_scenes.begin(), all_scenes.end(),
                       [&name](const std::shared_ptr<SceneWrapper>& s) {
                           return s->get_name() == name;
                       }),
        all_scenes.end()
    );
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

std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> PluginManager::get_shader_providers() {
    std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> types;
    for (const auto &item: get_plugins()) {
        auto pl_providers = item->get_shader_providers();
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

        constexpr const char* createSymbol = "plugin_create";
        constexpr const char* destroySymbol = "plugin_destroy";
        constexpr const char* apiVersionSymbol = "plugin_get_api_version";

        std::string legacyCreateSymbol = "create" + libName;
        std::string legacyDestroySymbol = "destroy" + libName;
        constexpr const char* legacyApiVersionSymbol = "get_api_version";

        int (*get_api_version)() = nullptr;
        dlerror();
        get_api_version = reinterpret_cast<int (*)()>(dlsym(dlhandle, apiVersionSymbol));
        const char *api_error = dlerror();
        if (api_error != nullptr || get_api_version == nullptr) {
            dlerror();
            get_api_version = reinterpret_cast<int (*)()>(dlsym(dlhandle, legacyApiVersionSymbol));
            api_error = dlerror();
        }

        if (api_error != nullptr || get_api_version == nullptr) {
            error("Plugin '{}' does not export API version symbol ('{}' or '{}')", plPath.string(), apiVersionSymbol, legacyApiVersionSymbol);
            dlclose(dlhandle);
            continue;
        }

        const int api_version = get_api_version();
        if (api_version != MATRIX_PLUGIN_API_VERSION) {
            error("Plugin '{}' has incompatible matrix API version {} (expected {})", plPath.string(), api_version, MATRIX_PLUGIN_API_VERSION);
            dlclose(dlhandle);
            continue;
        }

        // Clear any existing errors before dlsym
        dlerror();
        BasicPlugin *(*create)() = (BasicPlugin *(*)()) (dlsym(dlhandle, createSymbol));
        const char *dlsym_error = dlerror();
        if (dlsym_error != nullptr || create == nullptr) {
            dlerror();
            create = (BasicPlugin *(*)()) (dlsym(dlhandle, legacyCreateSymbol.c_str()));
            dlsym_error = dlerror();
        }

        if (dlsym_error != nullptr || create == nullptr) {
            error("Symbol lookup error in plugin '{}': {}", plPath.string(), dlsym_error != nullptr ? dlsym_error : "unknown");
            error("Expected symbol '{}' or '{}' not found", createSymbol, legacyCreateSymbol);
            dlclose(dlhandle);
            continue; // Skip this plugin and try the next one
        }

        // Verify destroy function exists before creating plugin
        dlerror();
        void *destroy_sym = dlsym(dlhandle, destroySymbol);
        const char *destroy_error = dlerror();
        std::string resolvedDestroySymbol = destroySymbol;
        if (destroy_error != nullptr || destroy_sym == nullptr) {
            dlerror();
            destroy_sym = dlsym(dlhandle, legacyDestroySymbol.c_str());
            destroy_error = dlerror();
            resolvedDestroySymbol = legacyDestroySymbol;
        }

        if (destroy_error != nullptr || destroy_sym == nullptr) {
            error("Destroy function '{}' or '{}' not found in plugin '{}'", destroySymbol, legacyDestroySymbol, plPath.string());
            dlclose(dlhandle);
            continue;
        }

        try {
            BasicPlugin *p = create();
            Dl_info dl_info;
            dladdr((void *) create, &dl_info);

            p->_plugin_location = dl_info.dli_fname;
            trace("Successfully loaded plugin {}", plPath.string());

            PluginInfo info = {
                .handle = dlhandle,
                .destroyFnName = resolvedDestroySymbol,
                .plugin = p,
            };
            loaded_plugins.emplace_back(info);
        } catch (const std::exception &e) {
            error("Failed to initialize plugin '{}': {}", plPath.string(), e.what());
            dlclose(dlhandle);
        }
    }

    trace("Loaded a total of {} plugins.", loaded_plugins.size());
    trace("Loading providers to register...");

    initialized = true;
}

PluginManager *PluginManager::instance() {
    static PluginManager instance;
    return &instance;
}
