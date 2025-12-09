#include "VideoPlugin.h"
#include "scenes/VideoScene.h"
#include <spdlog/spdlog.h>

using namespace Plugins;

extern "C" PLUGIN_EXPORT BasicPlugin *create() { return new VideoPlugin(); }

extern "C" PLUGIN_EXPORT void destroy(BasicPlugin *c) { delete c; }

vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>>
VideoPlugin::create_scenes() {
  vector<std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>> scenes;
  scenes.push_back(std::unique_ptr<SceneWrapper, void (*)(SceneWrapper *)>(
      new Scenes::VideoSceneWrapper(), [](SceneWrapper *s) { delete s; }));
  return scenes;
}

vector<std::unique_ptr<ImageProviderWrapper, void (*)(ImageProviderWrapper *)>>
VideoPlugin::create_image_providers() {
  return {};
}

vector<
    std::unique_ptr<ShaderProviderWrapper, void (*)(ShaderProviderWrapper *)>>
VideoPlugin::create_shader_providers() {
  return {};
}

bool VideoPlugin::on_udp_packet(const uint8_t pluginId,
                                const uint8_t *packetData, const size_t size) {
  std::lock_guard<std::mutex> lock(dataMutex);
  data.assign(packetData, packetData + size);
  return true;
}

std::optional<std::vector<std::string>> VideoPlugin::on_websocket_open() {
  return std::optional<std::vector<std::string>>({"video"});
}

void VideoPlugin::on_websocket_message(const std::string &message) {
  if (message.starts_with("status:")) {
    std::lock_guard<std::mutex> lock(statusMutex);
    status = message.substr(7);
    spdlog::info("VideoPlugin status update: {}", status);
  }
}
