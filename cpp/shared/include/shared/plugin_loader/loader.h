#pragma once

#include <thread>
#include <vector>
#include <typeinfo>
#include "plugin.h"
#include "shared/config/image_providers/general.h"

namespace Plugins {
    class PluginManager {
    protected:
        static PluginManager *instance_;

    private:
        /// Handle, Dn, Plugin
        std::vector<std::tuple<void *, string, Plugins::BasicPlugin *>> loaded_plugins;

        // Key: Type Value: Name
        std::map<const char*, string> img_provider_map = {};
        std::map<const char*, string> scene_provider_map = {};
        bool initialized = false;

        explicit PluginManager();

    public:

        PluginManager(PluginManager &other) = delete;
        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();
        void initialize();
        void terminate();

        std::optional<string> get_name_of_provider(ImageProviders::General *provider);
        std::optional<string> get_name_of_scene(Scenes::Scene *provider);
        std::vector<Plugins::BasicPlugin *> get_plugins();
        std::vector<Plugins::SceneWrapper *> get_scenes();
        std::vector<Plugins::ImageProviderWrapper *> get_image_providers();
    };
}