#include "VideoScene.h"
#include <random>
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <spdlog/spdlog.h>

using namespace Scenes;
std::string VideoScene::lastUrlSent = "";

std::unique_ptr<Scenes::Scene, void (*)(Scenes::Scene *)>
VideoSceneWrapper::create() {
  return {new VideoScene(), [](Scenes::Scene *scene) { delete scene; }};
}

VideoScene::VideoScene() : plugin(nullptr) {
  auto plugins = Plugins::PluginManager::instance()->get_plugins();
  for (auto &p : plugins) {
    if (auto v = dynamic_cast<VideoPlugin *>(p)) {
      plugin = v;
      break;
    }
  }

  if (!plugin) {
    spdlog::error("VideoScene: Failed to find Video plugin");
  }
}

string VideoScene::get_name() const { return "video"; }

void VideoScene::register_properties() {
  add_property(video_urls);
  add_property(random_playback);
}

void VideoScene::after_render_stop(RGBMatrixBase *matrix) {
  showing_loading_animation = false;
  // We might want to force a URL resend on next start if needed,
  // but preserving state is also fine.
  lastUrlSent = "";
}

void VideoScene::render_loading_animation() {
  const int width = Constants::width;
  const int height = Constants::height;

  offscreen_canvas->Fill(0, 0, 0);

  // Simple loading bar
  int barWidth = width * 0.8;
  int barHeight = 10;
  int x = (width - barWidth) / 2;
  int y = (height - barHeight) / 2;

  // Background
  for (int i = 0; i < barWidth; ++i) {
    for (int j = 0; j < barHeight; ++j) {
      offscreen_canvas->SetPixel(x + i, y + j, 50, 50, 50);
    }
  }

  // Foreground (animating)
  int progress = (loading_animation_frame % 100);
  int fillWidth = (barWidth * progress) / 100;

  for (int i = 0; i < fillWidth; ++i) {
    for (int j = 0; j < barHeight; ++j) {
      offscreen_canvas->SetPixel(x + i, y + j, 0, 255, 0);
    }
  }

  loading_animation_frame++;
}

bool VideoScene::render(RGBMatrixBase *matrix) {
  if (!plugin)
    return false;

  auto urls = video_urls->get();
  if (urls.empty()) {
    offscreen_canvas->Fill(50, 0, 0); // Dim red for no config
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
  }

  // Check status
  std::string status = plugin->get_status();

  if (status == "downloading" || status == "processing") {
    render_loading_animation();
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
  }

  // Select URL logic
  tmillis_t now = GetTimeInMillis();
  if (lastUrlSent.empty() ||
      (now - last_switch_time > 30000 && urls.size() > 1)) { // 30 seconds

    if (!lastUrlSent.empty()) {
      if (random_playback->get()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, urls.size() - 1);
        current_video_index = distrib(gen);
      } else {
        current_video_index = (current_video_index + 1) % urls.size();
      }
    } else { // This block was missing in the snippet, added to handle initial
             // selection
      if (random_playback->get()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, urls.size() - 1);
        current_video_index = distrib(gen);
      } else {
        current_video_index = 0; // Start with the first video if not random
      }
    }

    std::string targetUrl = urls[current_video_index];
    if (targetUrl != lastUrlSent) {
      plugin->send_msg_to_desktop("url:" + targetUrl);
      lastUrlSent = targetUrl;
      last_switch_time = now;
    }
  }

  // Render Frame
  const auto pixels = plugin->get_data();
  if (pixels.empty()) {
    // loading or waiting
    render_loading_animation();
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
    return true;
  }

  const int width = Constants::width;
  const int height = Constants::height;
  const uint8_t *data = pixels.data();
  const int max_pixels = pixels.size() / 3;
  const int limit = std::min(width * height, max_pixels);

  for (int idx = 0; idx < limit; ++idx) {
    int x = idx % width;
    int y = idx / width; // Standard scanline order from ffmpeg rgb24 usually
                         // (top-left to bottom-right)
    // But matrix might be different. Shadertoy used y = height - 1 - ...
    // ffmpeg rawvideo is top-left origin.
    // rpi-rgb-led-matrix is top-left origin usually.

    int i = idx * 3;
    offscreen_canvas->SetPixel(x, y, data[i], data[i + 1], data[i + 2]);
  }

  offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
  return true;
}
