#pragma once

#include <thread>
#include <vector>
#include <typeinfo>
#include "plugin/main.h"
#include "shared/config/image_providers/general.h"

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

        bool initialized = false;

        explicit PluginManager();

    public:

        PluginManager(PluginManager &other) = delete;
        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();
        void initialize();
        void terminate();

        std::vector<Plugins::BasicPlugin*> get_plugins();
        std::vector<std::shared_ptr<SceneWrapper>> get_scenes();
        std::vector<std::shared_ptr<Plugins::ImageProviderWrapper>> get_image_providers();
    };
}