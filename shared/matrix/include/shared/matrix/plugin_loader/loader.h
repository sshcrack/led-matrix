#pragma once

#include <vector>
#include <mutex>
#include "shared/common/plugin_loader/PluginLoader.h"
#include "shared/matrix/plugin/main.h"
#include "shared/matrix/config/image_providers/general.h"
#include "shared/matrix/config/shader_providers/general.h"

namespace Plugins {
    class PluginManager : public PluginLoader<BasicPlugin> {
    private:
        static PluginManager *instance_;
        std::vector<std::shared_ptr<SceneWrapper>> all_scenes;
        std::mutex scenes_mutex;
        bool scenes_initialized = false;

        explicit PluginManager();

    public:
        PluginManager(PluginManager &other) = delete;
        void operator=(const PluginManager &) = delete;

        static PluginManager *instance();

        std::vector<Plugins::BasicPlugin*> get_plugins();

        std::vector<std::shared_ptr<SceneWrapper>> get_scenes();
        void add_scene(std::shared_ptr<SceneWrapper> scene);
        void remove_scene(const std::string& name);
        std::vector<std::shared_ptr<Plugins::ImageProviderWrapper>> get_image_providers();
        std::vector<std::shared_ptr<Plugins::ShaderProviderWrapper>> get_shader_providers();

        void delete_references();
    };
}
