#pragma once

#include "shared/matrix/plugin/main.h"
#include <mutex>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <thread>
#include <atomic>

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::ShaderProviderWrapper;
using Plugins::BasicPlugin;

class ShadertoyPlugin final : public BasicPlugin {
public:
    vector<std::unique_ptr<SceneWrapper, void (*)(Plugins::SceneWrapper *)> > create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper *)> > create_image_providers() override;

    vector<std::unique_ptr<ShaderProviderWrapper, void(*)(ShaderProviderWrapper *)> > create_shader_providers() override;

    bool on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                       const size_t size) override;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }

    std::optional<std::vector<std::string>> on_websocket_open() override;
    void on_websocket_message(const std::string &message) override;
    std::optional<std::string> after_server_init() override;
    std::optional<std::string> pre_exit() override;
    std::string add_custom_shader_scene(const std::filesystem::path &shader_file_path);
    std::string remove_custom_shader_scene(const std::filesystem::path &shader_file_path);

    std::vector<uint8_t> get_data() {
        std::lock_guard<std::mutex> lock(dataMutex);
        return data;
    }

private:
    std::mutex dataMutex;
    std::vector<uint8_t> data;
    std::mutex customSceneMutex;
    std::unordered_map<std::string, std::string> customSceneNamesByFile;
    std::thread watcher_thread_;
    std::atomic<bool> stop_watcher_{false};
    void watch_custom_shader_dir();
};
