#pragma once

#include "shared/matrix/plugin/main.h"
#include "LuaScene.h"
#include <thread>
#include <atomic>
#include <unordered_map>
#include <filesystem>

class ScriptedScenes : public Plugins::BasicPlugin {
public:
    ScriptedScenes() = default;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }

    vector<std::unique_ptr<Plugins::ImageProviderWrapper,
                            void (*)(Plugins::ImageProviderWrapper *)>>
    create_image_providers() override { return {}; }

    vector<std::unique_ptr<Plugins::SceneWrapper,
                            void (*)(Plugins::SceneWrapper *)>>
    create_scenes() override;

    std::optional<std::string> after_server_init() override;
    std::optional<std::string> pre_exit() override;

private:
    std::thread watcher_thread_;
    std::atomic<bool> stop_watcher_{false};
    std::unordered_map<std::string, std::filesystem::file_time_type> known_files_;

    void watch_directory();
};
