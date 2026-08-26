#include "SpotifyMVScene.h"
#include <shared/matrix/canvas_consts.h>
#include <shared/matrix/plugin_loader/loader.h>
#include <shared/matrix/runtime_inputs.h>
#include <shared/matrix/utils/LoadingAnimation.h>
#include <spdlog/spdlog.h>

using namespace Scenes;


std::unique_ptr<Scenes::Scene>
SpotifyMVSceneWrapper::create()
{
  return std::make_unique<SpotifyMVScene>();
}

SpotifyMVScene::SpotifyMVScene() : plugin_(nullptr)
{
  auto plugins = Plugins::PluginManager::instance()->get_plugins();
  for (auto &p : plugins)
  {
    if (auto v = dynamic_cast<SpotifyMVPlugin *>(p))
    {
      plugin_ = v;
      break;
    }
  }
  if (!plugin_)
  {
    spdlog::error("SpotifyMVScene: Failed to find SpotifyMV plugin");
  }
}

void SpotifyMVScene::register_properties()
{
  add_property(search_suffix);
  add_property(fallback_to_lyric_video);
}

Scenes::SceneDescriptor SpotifyMVScene::get_descriptor() const
{
  auto d = Scene::get_descriptor();
  d.automatic_eligible = true;
  d.family = "spotify-video";
  d.tags = {"music", "media", "spotify", "spotify-video", "cinematic", "motion"};
  d.intensity = 0.70f;
  d.motion = 0.86f;
  d.music_affinity = 1.0f;
  // The Pi only copies already-decoded RGB frames. Downloading/decoding is
  // deliberately paid by the desktop plugin, so this is a cheap Pi scene.
  d.performance_cost = 0.18f;
  return d;
}

void SpotifyMVScene::prepare_runtime(const RuntimeInputs::Snapshot& snapshot)
{
  if (!plugin_)
    return;

  if (!snapshot.available(RuntimeInputIds::SpotifyPlayback))
  {
    if (!last_track_id_sent_.empty())
    {
      plugin_->send_msg_to_desktop("stop");
      plugin_->clear_last_track_message();
      last_track_id_sent_.clear();
    }
    return;
  }

  const auto track_id = snapshot.text(RuntimeInputIds::SpotifyPlayback, "track_id").value_or("");
  const auto song = snapshot.text(RuntimeInputIds::SpotifyPlayback, "track").value_or("");
  const auto artist = snapshot.text(RuntimeInputIds::SpotifyPlayback, "artist").value_or("");
  if (track_id.empty() || (song.empty() && artist.empty()) || track_id == last_track_id_sent_)
    return;

  const auto progress_ms = static_cast<long>(
      snapshot.number(RuntimeInputIds::SpotifyPlayback, "progress_ms").value_or(0.0));
  const auto duration_ms = static_cast<long>(
      snapshot.number(RuntimeInputIds::SpotifyPlayback, "duration_ms").value_or(0.0));
  const auto suffix = search_suffix->get();
  const auto fallback = fallback_to_lyric_video->get() ? "true" : "false";
  const auto track_msg = "track:" + track_id + ":" + song + "\n" + artist + "\n" + suffix + "\n" + fallback + "\n"
      + std::to_string(progress_ms) + "\n" + std::to_string(duration_ms);

  // Hidden preparation deliberately does not send pixel packets. The desktop
  // only emits the large UDP stream when `spotifymv` is the active scene.
  plugin_->send_msg_to_desktop(track_msg);
  plugin_->set_last_track_message(track_msg);
  plugin_->flush_status();
  last_track_id_sent_ = track_id;
  loading_frame_ = 0;
}

void SpotifyMVScene::after_render_stop()
{
  if (plugin_)
  {
    plugin_->send_msg_to_desktop("stop");
    plugin_->flush_status();
    plugin_->clear_last_track_message();
  }
  // Remember which track was just stopped so hidden automatic preparation does
  // not immediately download it again. A visible manual re-entry below can
  // explicitly restart the same track if desired.
  loading_frame_ = 0;
}

void SpotifyMVScene::render_loading(rgb_matrix::FrameCanvas *canvas, bool is_searching)
{
  uint8_t r = 0, g = 255, b = 0;
  if (is_searching)
  {
    r = 30;
    g = 215;
    b = 96;
  }
  LoadingAnimation::render(canvas, loading_frame_++, r, g, b);
}

bool SpotifyMVScene::render(rgb_matrix::FrameCanvas *canvas)
{
  if (!plugin_)
  {
    spdlog::warn("[SpotifyMVScene] No plugin instance — SpotifyMVScene will not render");
    hold_current_frame();
    return false;
  }

  const auto runtime_inputs = RuntimeInputs::snapshot();
  const auto active_track_id = runtime_inputs.text(RuntimeInputIds::SpotifyPlayback, "track_id").value_or("");
  if (!active_track_id.empty() && active_track_id == last_track_id_sent_ && plugin_->get_status() == "idle")
    last_track_id_sent_.clear();
  prepare_runtime(runtime_inputs);
  if (!runtime_inputs.available(RuntimeInputIds::SpotifyPlayback))
  {
    canvas->Fill(0, 0, 0);
    return true;
  }

  auto frame = plugin_->get_frame();
  if (!frame.empty())
  {
    const int width = Constants::width;
    const int height = Constants::height;
    const uint8_t *data = frame.data();
    const int max_pixels = frame.size() / 3;
    const int limit = std::min(width * height, max_pixels);

    for (int idx = 0; idx < limit; ++idx)
    {
      int x = idx % width;
      int y = idx / width;
      int i = idx * 3;
      canvas->SetPixel(x, y, data[i], data[i + 1], data[i + 2]);
    }

    auto status = plugin_->get_status();
    if (status == "pending" || status == "searching" || status == "downloading" || status == "processing")
    {
      LoadingAnimation::render_overlay(canvas, loading_frame_++, 30, 215, 96);
    }

    return true;
  }

  auto status = plugin_->get_status();
  if (status == "playing")
  {
    // The desktop has decoded a frame but the bulk UDP stream may only just
    // have been enabled for the transition. Keep the outgoing matrix frame
    // until the first current-track packet arrives instead of exposing a
    // loader or stale pixels.
    hold_current_frame();
    return true;
  }
  if (status == "idle")
  {
    canvas->Fill(0, 0, 0);
    return true;
  }
  if (status == "error")
  {
    canvas->Fill(20, 0, 0);
    return true;
  }
  if (status == "searching" || status == "downloading" || status == "processing" || status == "pending")
  {
    render_loading(canvas, status == "searching");
    return true;
  }
  if (plugin_->is_stale())
  {
    canvas->Fill(10, 0, 20);
    return true;
  }

  render_loading(canvas, false);
  return true;
}
