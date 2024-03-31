#pragma once

#include <thread>
#include <vector>
#include "plugin.h"
#ifndef PLUGIN_TEST
#include "matrix_control/scene/Scene.h"
#include "config/image_providers/general.h"
#endif

namespace Plugins {
    class PluginManager {
    private:
        /// Handle, Dn, Plugin
        std::vector<std::tuple<void *, string, Plugins::BasicPlugin *>> loaded_plugins;

    public:
        explicit PluginManager();

        void terminate();

        std::vector<Plugins::BasicPlugin *> get_plugins();

#ifndef PLUGIN_TEST
        std::vector<Plugins::SceneWrapper*> get_scenes();
        std::vector<Plugins::ImageTypeWrapper*> get_image_type();
#endif
    };
}