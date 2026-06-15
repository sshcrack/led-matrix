#include "SpotifyMVScene.h"
#include "../../../SpotifyScenes/matrix/manager/spotify.h"
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/plugin_registry.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <spdlog/spdlog.h>

using namespace Scenes;

namespace {
  Spotify* resolve_spotify() {
    static Spotify* cached = nullptr;
    static bool tried = false;
    static bool warned = false;
    if (!tried) {
      tried = true;
      cached = static_cast<Spotify*>(PluginRegistry::get("spotify"));
      if (cached == nullptr && !warned) {
        warned = true;
        spdlog::warn("[SpotifyMVScene] SpotifyScenes unavailable (disabled or missing credentials) — track detection disabled");
      }
    }
    return cached;
  }
}

std::unique_ptr<Scenes::Scene, void(*)(Scenes::Scene*)>
SpotifyMVSceneWrapper::create() {
  return {new SpotifyMVScene(), [](Scenes::Scene* scene) { delete scene; }};
}

SpotifyMVScene::SpotifyMVScene() : plugin_(nullptr) {
  auto plugins = Plugins::PluginManager::instance()->get_plugins();
  for (auto& p : plugins) {
    if (auto v = dynamic_cast<SpotifyMVPlugin*>(p)) {
      plugin_ = v;
      break;
    }
  }
  if (!plugin_) {
    spdlog::error("SpotifyMVScene: Failed to find SpotifyMV plugin");
  }
}

void SpotifyMVScene::register_properties() {
  add_property(search_suffix);
  add_property(fallback_to_lyric_video);
}

void SpotifyMVScene::after_render_stop() {
  if (plugin_) {
    plugin_->send_msg_to_desktop("spotifymv:stop");
    plugin_->flush_status();
  }
  last_track_id_sent_ = "";
  loading_frame_ = 0;
}

void SpotifyMVScene::render_loading(rgb_matrix::FrameCanvas* canvas, bool is_searching) {
  const int width = Constants::width;
  const int height = Constants::height;

  canvas->Fill(0, 0, 0);

  int barWidth = width * 0.8;
  int barHeight = 10;
  int x = (width - barWidth) / 2;
  int y = (height - barHeight) / 2;

  for (int i = 0; i < barWidth; ++i)
    for (int j = 0; j < barHeight; ++j)
      canvas->SetPixel(x + i, y + j, 50, 50, 50);

  int progress = (loading_frame_ % 100);
  int fillWidth = (barWidth * progress) / 100;

  uint8_t r, g, b;
  if (is_searching) {
    r = 30;  g = 215; b = 96;  // Spotify green
  } else {
    r = 0;   g = 255; b = 0;   // Standard green
  }

  for (int i = 0; i < fillWidth; ++i)
    for (int j = 0; j < barHeight; ++j)
      canvas->SetPixel(x + i, y + j, r, g, b);

  loading_frame_++;
}

bool SpotifyMVScene::render(rgb_matrix::FrameCanvas* canvas) {
  if (!plugin_) {
    spdlog::warn("[SpotifyMVScene] No plugin instance — SpotifyMVScene will not render");
    return false;
  }

  auto* sp = resolve_spotify();
  if (sp == nullptr) {
    canvas->Fill(0, 0, 0);
    return true;
  }

  auto state_opt = sp->get_currently_playing();
  if (!state_opt || !state_opt->is_playing()) {
    canvas->Fill(0, 0, 0);
    return true;
  }

  auto track = state_opt->get_track();
  auto track_id = track.get_id().value_or("");
  if (track_id != last_track_id_sent_) {
    auto song = track.get_song_name().value_or("");
    auto artist = track.get_artist_name().value_or("");
    auto suffix = search_suffix->get();
    auto fb = fallback_to_lyric_video->get() ? "true" : "false";
    plugin_->send_msg_to_desktop(
        "spotifymv:track:" + track_id + ":" + song + "\n" + artist + "\n" + suffix + "\n" + fb);
    last_track_id_sent_ = track_id;
    loading_frame_ = 0;
    plugin_->flush_status();
  }

  auto status = plugin_->get_status();
  if (status == "idle") {
    canvas->Fill(0, 0, 0);
    return true;
  }
  if (status == "error") {
    canvas->Fill(20, 0, 0);
    return true;
  }
  if (status == "searching" || status == "downloading" || status == "processing") {
    render_loading(canvas, status == "searching");
    return true;
  }

  auto frame = plugin_->get_frame();
  if (frame.empty()) {
    render_loading(canvas, false);
    return true;
  }

  const int width = Constants::width;
  const int height = Constants::height;
  const uint8_t* data = frame.data();
  const int max_pixels = frame.size() / 3;
  const int limit = std::min(width * height, max_pixels);

  for (int idx = 0; idx < limit; ++idx) {
    int x = idx % width;
    int y = idx / width;
    int i = idx * 3;
    canvas->SetPixel(x, y, data[i], data[i + 1], data[i + 2]);
  }

  return true;
}
