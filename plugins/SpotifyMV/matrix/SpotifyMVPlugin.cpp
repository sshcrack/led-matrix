#include "SpotifyMVPlugin.h"
#include "scenes/SpotifyMVScene.h"
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>
#include <spdlog/spdlog.h>

using namespace Plugins;

REGISTER_PLUGIN(SpotifyMV, SpotifyMVPlugin)

std::vector<std::unique_ptr<SceneWrapper>>
SpotifyMVPlugin::create_scenes() {
  std::vector<std::unique_ptr<SceneWrapper>> scenes;
  scenes.push_back(std::make_unique<Scenes::SpotifyMVSceneWrapper>());
  return scenes;
}

std::vector<std::unique_ptr<ImageProviderWrapper>>
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

  // Ask desktop clients to prove that the external video toolchain is usable.
  // Do not clear readiness here: scene-worker websocket connections use this same
  // route and must not invalidate the main desktop client's capability heartbeat.
  msgs.push_back("tools:probe");

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
  if (message == "tools:ready") {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      tools_ready_ = true;
    }
    publish_runtime_state();
    return;
  }
  if (message == "tools:error") {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      tools_ready_ = false;
      first_frame_ready_ = false;
    }
    publish_runtime_state();
    return;
  }
  if (message.starts_with("track:preparing:")) {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      prepared_track_id_ = message.substr(std::string("track:preparing:").size());
      first_frame_ready_ = false;
      status_ = "preparing";
    }
    {
      std::lock_guard<std::mutex> lock(frame_mutex_);
      frame_.clear();
      last_frame_time_ = {};
    }
    publish_runtime_state();
    return;
  }
  if (message.starts_with("track:ready:")) {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      prepared_track_id_ = message.substr(std::string("track:ready:").size());
      first_frame_ready_ = !prepared_track_id_.empty();
      status_ = first_frame_ready_ ? "playing" : "idle";
    }
    publish_runtime_state();
    return;
  }
  if (message.starts_with("track:error:")) {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      prepared_track_id_ = message.substr(std::string("track:error:").size());
      first_frame_ready_ = false;
      status_ = "error";
    }
    publish_runtime_state();
    return;
  }
  if (message == "track:idle") {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      prepared_track_id_.clear();
      first_frame_ready_ = false;
      status_ = "idle";
    }
    {
      std::lock_guard<std::mutex> lock(frame_mutex_);
      frame_.clear();
      last_frame_time_ = {};
    }
    publish_runtime_state();
    return;
  }
  if (message.starts_with("status:")) {
    {
      std::lock_guard<std::mutex> lock(status_mutex_);
      status_ = message.substr(7);
      if (status_ == "error")
        first_frame_ready_ = false;
      spdlog::info("SpotifyMVPlugin status update: {}", status_);
    }
    publish_runtime_state();
  }
}

void SpotifyMVPlugin::flush_status() {
  std::lock_guard<std::mutex> lock(status_mutex_);
  status_ = "idle";
}

void SpotifyMVPlugin::clear_last_track_message() {
  std::lock_guard<std::mutex> lock(track_msg_mutex_);
  last_track_message_.clear();
}

void SpotifyMVPlugin::publish_runtime_state() {
  bool tools_ready = false;
  std::string status;
  std::string track_id;
  bool first_frame_ready = false;
  {
    std::lock_guard<std::mutex> lock(status_mutex_);
    tools_ready = tools_ready_;
    status = status_;
    track_id = prepared_track_id_;
    first_frame_ready = first_frame_ready_;
  }

  RuntimeInputs::Signals signals{
      {"tools_ready", tools_ready},
      {"state", status},
      {"first_frame_ready", first_frame_ready},
  };
  if (!track_id.empty())
    signals.emplace("track_id", track_id);

  if (tools_ready) {
    // This is capability/preparation state, not a heartbeat. Automatic Mode can
    // intentionally wait several seconds for a clean insertion point, so a TTL
    // here can expire a perfectly prepared MV before the Director may select it.
    // Desktop disconnect eligibility is tracked separately by the Desktop input.
    RuntimeInputs::set_available(RuntimeInputIds::SpotifyMVReady, true, std::move(signals));
  } else {
    RuntimeInputs::set_available(RuntimeInputIds::SpotifyMVReady, false, std::move(signals));
  }
}
