#pragma once

#include "shared/matrix/plugin/main.h"
#include <atomic>
#include <filesystem>
#include <thread>
#include <unordered_map>

class ScriptedScenes : public Plugins::BasicPlugin {
public:
  ScriptedScenes() = default;

  std::string get_plugin_name() const override { return PLUGIN_NAME; }

  vector<std::unique_ptr<Plugins::ImageProviderWrapper,
                         void (*)(Plugins::ImageProviderWrapper *)>>
  create_image_providers() override {
    return {};
  }

  vector<
      std::unique_ptr<Plugins::SceneWrapper, void (*)(Plugins::SceneWrapper *)>>
  create_scenes() override;

  std::optional<std::string> after_server_init() override;
  std::optional<std::string> pre_exit() override;

  bool on_udp_packet(const uint8_t pluginId, const uint8_t *packetData,
                     const size_t size) override;
  std::optional<std::vector<std::string>> on_websocket_open() override;

  std::vector<uint8_t> get_data();
  void set_active_script(std::filesystem::path script_file_path, std::string sceneName);
  std::string add_custom_lua_scene(const std::filesystem::path &script_path);
  std::string remove_custom_lua_scene(const std::filesystem::path &script_path);

private:
  std::thread watcher_thread_;
  std::atomic<bool> stop_watcher_{false};
  std::unordered_map<std::string, std::filesystem::file_time_type> known_files_;
  std::unordered_map<std::string, std::string> scene_name_by_path_;

  std::mutex dataMutex;
  std::vector<uint8_t> data;

  std::mutex activeScriptPathMutex;
  std::string active_scene_name;
  std::filesystem::path active_script_path;

  void watch_directory();
};
