#pragma once

#include <vector>
#include "shared/matrix/plugin/main.h"
#include "shared/matrix/config/image_providers/general.h"

namespace Plugins {
    struct PluginInfo {
        void* handle;
        std::string destroyFnName;

        BasicPlugin* plugin;

        std::vector<std::shared_ptr<SceneWrapper>> sceneWrappers;
        std::vector<std::shared_ptr<ImageProviderWrapper>> imageProviderWrappers;
    };

    class PluginManager {
    protected:
        static PluginManager *instance_;

    private:
        /// Handle, DestroyFunction, Plugin
        std::vector<PluginInfo> loaded_plugins;
        std::vector<std::shared_ptr<SceneWrapper>> all_scenes;

        bool initialized = false;

        explicit PluginManager();

    public:

        PluginManager(PluginManager &other) = delete;
        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();
        void initialize();
        void delete_references();
        void destroy_plugins();

        std::vector<Plugins::BasicPlugin*> get_plugins();

        std::vector<std::shared_ptr<SceneWrapper>> &get_scenes();
        std::vector<std::shared_ptr<Plugins::ImageProviderWrapper>> get_image_providers();
        
        // Dynamic plugin management for marketplace
        bool load_plugin(const std::string& plugin_path);
        bool unload_plugin(const std::string& plugin_id);
        void refresh_scenes_cache();
        bool is_plugin_loaded(const std::string& plugin_id) const;
    };
}