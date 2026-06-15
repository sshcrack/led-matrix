#include "SpotifyMVPlugin.h"
#include "scenes/SpotifyMVScene.h"
#include <spdlog/spdlog.h>

using namespace Plugins;

extern "C" PLUGIN_EXPORT SpotifyMVPlugin* createSpotifyMV() {
  return new SpotifyMVPlugin();
}

extern "C" PLUGIN_EXPORT void destroySpotifyMV(SpotifyMVPlugin* c) { delete c; }

std::vector<std::unique_ptr<SceneWrapper, void(*)(SceneWrapper*)>>
SpotifyMVPlugin::create_scenes() {
  std::vector<std::unique_ptr<SceneWrapper, void(*)(SceneWrapper*)>> scenes;
  scenes.push_back(std::unique_ptr<SceneWrapper, void(*)(SceneWrapper*)>(
      new Scenes::SpotifyMVSceneWrapper(), [](SceneWrapper* s) { delete s; }));
  return scenes;
}

std::vector<std::unique_ptr<ImageProviderWrapper, void(*)(ImageProviderWrapper*)>>
SpotifyMVPlugin::create_image_providers() {
  return {};
}

bool SpotifyMVPlugin::on_udp_packet(uint8_t pluginId, const uint8_t* data, size_t size) {
  if (pluginId != 0x04) return false;
  std::lock_guard<std::mutex> lock(frame_mutex_);
  frame_.assign(data, data + size);
  last_frame_time_ = std::chrono::steady_clock::now();
  return true;
}

std::optional<std::vector<std::string>> SpotifyMVPlugin::on_websocket_open() {
  std::vector<std::string> msgs;

  {
    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_ != "idle") {
      msgs.push_back("status:" + status_);
    }
  }

  {
    std::lock_guard<std::mutex> lock(track_msg_mutex_);
    if (!last_track_message_.empty()) {
      msgs.push_back(last_track_message_);
    }
  }

  if (msgs.empty())
    return std::nullopt;
  return msgs;
}

void SpotifyMVPlugin::on_websocket_message(const std::string& message) {
  if (message.starts_with("status:")) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_ = message.substr(7);
    spdlog::info("SpotifyMVPlugin status update: {}", status_);
  }
}

void SpotifyMVPlugin::flush_status() {
  std::scoped_lock lock(status_mutex_, track_msg_mutex_);
  status_ = "idle";
  last_track_message_.clear();
}
