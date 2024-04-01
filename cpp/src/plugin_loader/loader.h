#pragma once

#include <thread>
#include <vector>
#include "plugin.h"
#include "config/image_providers/general.h"

namespace Plugins {
    class PluginManager {
    protected:
        static PluginManager *instance_;

    private:
        /// Handle, Dn, Plugin
        std::vector<std::tuple<void *, string, Plugins::BasicPlugin *>> loaded_plugins;
        bool initialized = false;

        explicit PluginManager();

    public:

        PluginManager(PluginManager &other) = delete;

        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();

        void initialize();

        void terminate();

        std::vector<Plugins::BasicPlugin *> get_plugins();

        std::vector<Plugins::SceneWrapper *> get_scenes();

        std::vector<Plugins::ImageTypeWrapper *> get_image_type();
    };
}