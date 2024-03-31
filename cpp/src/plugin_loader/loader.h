#pragma once

#include <thread>
#include <vector>
#include "plugin.h"
#include "matrix_control/scene/Scene.h"
#include "config/image_providers/general.h"

namespace Plugins {
    class PluginManager {
    private:
        /// Handle, Dn, Plugin
        std::vector<std::tuple<void*, string, Plugins::BasicPlugin*>> loaded_plugins;

    public:
        explicit PluginManager();

        void terminate();
        std::vector<Plugins::SceneWrapper*> get_scenes();
        std::vector<Plugins::ImageTypeWrapper*> get_image_type();
        std::vector<Plugins::BasicPlugin*> get_plugins();
    };
}