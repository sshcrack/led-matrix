#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "shared/matrix/frame_stream.h"
#include "shared/matrix/plugin/main.h"

using Plugins::SceneWrapper;
using Plugins::ImageProviderWrapper;
using Plugins::ShaderProviderWrapper;
using Plugins::BasicPlugin;

class ShadertoyPlugin final : public BasicPlugin {
public:
    vector<std::unique_ptr<SceneWrapper> > create_scenes() override;

    vector<std::unique_ptr<ImageProviderWrapper> > create_image_providers() override;

    vector<std::unique_ptr<ShaderProviderWrapper> > create_shader_providers() override;

    vector<std::unique_ptr<Previews::DataProvider> > create_preview_data_providers() override;

    bool on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                       const size_t size) override;

    std::string get_plugin_name() const override { return PLUGIN_NAME; }
    [[nodiscard]] bool requires_single_desktop_producer() const override { return true; }

    std::optional<std::vector<std::string>> on_websocket_open() override;
    void on_websocket_message(const std::string &message) override;
    std::optional<std::string> after_server_init() override;
    std::optional<std::string> pre_exit() override;
    std::string add_custom_shader_scene(const std::filesystem::path &shader_file_path);
    std::string remove_custom_shader_scene(const std::filesystem::path &shader_file_path);

    /// Shader frames from the desktop (top-down RGB888).
    [[nodiscard]] const FrameStream::Receiver& frames() const { return frames_; }

    void set_last_sent_message(const std::string& message) {
        std::lock_guard<std::mutex> lock(lastMsgMutex);
        last_sent_message = message;
    }

private:
    std::mutex lastMsgMutex;
    std::string last_sent_message;
    // Older desktops send bottom-up OpenGL readbacks in one datagram.
    FrameStream::Receiver frames_{"Shadertoy", true};
    std::mutex customSceneMutex;
    std::unordered_map<std::string, std::string> customSceneNamesByFile;
    std::thread watcher_thread_;
    std::atomic<bool> stop_watcher_{false};
    void watch_custom_shader_dir();
};
