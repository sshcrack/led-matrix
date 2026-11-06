#pragma once

#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>
#include "shared/common/plugin_loader/PluginLoader.h"
#include "shared/matrix/plugin/main.h"
#include "shared/matrix/config/image_providers/general.h"
#include "shared/matrix/config/shader_providers/general.h"

namespace Plugins {
    struct RegistryValidationReport {
        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        [[nodiscard]] bool ok() const { return errors.empty(); }
        [[nodiscard]] nlohmann::json to_json() const {
            return {
                {"ok", ok()},
                {"errors", errors},
                {"warnings", warnings}
            };
        }
    };
    class PluginManager : public PluginLoader<BasicPlugin> {
    private:
        static PluginManager *instance_;
        std::vector<std::shared_ptr<SceneWrapper>> all_scenes;
        /// Guards all_scenes, the cache flags and validation_report_. The
        /// Shadertoy watcher mutates the registry from a background thread
        /// while HTTP handlers read it, so every access must hold this lock.
        mutable std::mutex scenes_mutex;
        bool scenes_initialized = false;
        size_t scenes_plugin_count_ = 0;
        RegistryValidationReport validation_report_;

        explicit PluginManager();

        /// Validates `scenes` against the loaded plugins without touching any
        /// registry state. Caller must hold scenes_mutex.
        RegistryValidationReport build_report(const std::vector<std::shared_ptr<SceneWrapper>> &scenes);

        /// Validates a candidate scene list and publishes it as the registry
        /// only when it has no errors. The report is always recorded so
        /// diagnostics can explain a rejection. Caller must hold scenes_mutex.
        RegistryValidationReport publish_if_valid(std::vector<std::shared_ptr<SceneWrapper>> candidate);

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

        RegistryValidationReport validate_registry(bool throw_on_error = false);
        [[nodiscard]] RegistryValidationReport get_validation_report() const;

        /// Release scene wrapper references before unloading plugin DSOs.
        void delete_references();
    };
}
