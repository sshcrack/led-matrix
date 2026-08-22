#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

#include <nlohmann/json.hpp>

namespace rgb_matrix { class FrameCanvas; }
namespace Scenes { class Scene; }

namespace RemoteRender {

struct Status {
    bool requested = false;
    bool frame_fresh = false;
    std::uint32_t session = 0;
    std::uint32_t last_sequence = 0;
    double frame_age_ms = 0.0;
    std::string scene;
    std::string renderer;
};

using CommandSender = std::function<void(const nlohmann::json &)>;

/// Register the matrix-side plugin transport used to send control messages to
/// the desktop worker. Rendering code intentionally knows nothing about the
/// WebSocket implementation.
void set_command_sender(CommandSender sender);
void clear_command_sender();

/// Start (or reuse) a remote-render session for this scene. The scene's
/// canonical property JSON is sent to the desktop so remote rendering remains
/// an internal placement decision rather than separate user configuration.
std::optional<std::uint32_t> request_scene(
    const Scenes::Scene &scene, int width, int height, int target_fps);

/// Keep music-reactive remote renderers fed from the same authoritative audio
/// snapshot as local scenes. Internally rate-limited to avoid control chatter.
void publish_audio(std::uint32_t session);

/// Called by the matrix RenderOffload UDP plugin. Frames are latest-wins: stale
/// sessions and out-of-order sequences are discarded immediately.
bool submit_frame(std::uint32_t session, std::uint32_t sequence,
                  int width, int height, std::span<const std::uint8_t> rgb);

/// Copy the latest complete RGB888 frame into a canvas if it is recent enough.
/// No queue is maintained, so network jitter cannot build up latency.
bool copy_latest(std::uint32_t session, rgb_matrix::FrameCanvas *canvas,
                 int width, int height, double max_age_ms = 300.0);

/// Return a start command for a newly reconnected desktop, if a session is
/// currently requested.
std::optional<nlohmann::json> reconnect_command();

Status status();
void stop();

} // namespace RemoteRender
