#pragma once

#include <vector>
#include "shared/matrix/plugin/main.h"
#include "shared/matrix/config/image_providers/general.h"
#include "shared/matrix/config/shader_providers/general.h"
#include <shared_mutex>

namespace Plugins {
    struct PluginInfo {
        void* handle;
        std::string destroyFnName;

        BasicPlugin* plugin;

        std::vector<std::shared_ptr<SceneWrapper>> sceneWrappers;
        std::vector<std::shared_ptr<ImageProviderWrapper>> imageProviderWrappers;
        std::vector<std::shared_ptr<ShaderProviderWrapper>> shaderProviderWrappers;
    };

    class PluginManager {
    private:
        /// Handle, DestroyFunction, Plugin
        std::vector<PluginInfo> loaded_plugins;
        std::vector<std::shared_ptr<SceneWrapper>> all_scenes;
        std::shared_mutex scenes_mutex;
        bool scenes_initialized = false;

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

        std::vector<std::shared_ptr<SceneWrapper>> get_scenes();
        void add_scene(std::shared_ptr<SceneWrapper> scene);
        void remove_scene(const std::string& name);
        std::vector<std::shared_ptr<Plugins::ImageProviderWrapper>> get_image_providers();
        std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> get_shader_providers();
    };
}
