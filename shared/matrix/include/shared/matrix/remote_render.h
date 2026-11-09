#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
    bool worker_available = false;
    /// At least one advertised scene has a compatible data contract.
    bool worker_compatible = false;
    int worker_protocol = 0;
    std::size_t worker_scene_count = 0;
    std::size_t worker_compatible_scene_count = 0;
};

using CommandSender = std::function<void(const nlohmann::json &)>;

/// Register the matrix-side plugin transport used to send control messages to
/// the desktop worker. Rendering code intentionally knows nothing about the
/// WebSocket implementation.
void set_command_sender(CommandSender sender);
void clear_command_sender();

using SceneContracts = std::map<std::string, int>;

/// Publish the validated local registry, including dynamic additions/removals.
/// Registry metadata is copied; no plugin objects are retained by the transport.
void set_local_scene_contracts(SceneContracts contracts);

/// A worker advertises only offloadable scene IDs and their data contract versions.
/// A matching protocol and requested scene contract are required; unknown,
/// missing and nonpositive contracts are never eligible.
void report_worker_heartbeat(int protocol_version, SceneContracts scene_contracts = {});
[[nodiscard]] bool worker_available(std::string_view scene = {});

/// Start (or reuse) a remote-render session for this scene. The scene's
/// canonical property JSON is sent to the desktop so remote rendering remains
/// an internal placement decision rather than separate user configuration.
/// A nonempty transition starts a new session carrying {name, duration_ms, from}
/// (RGB888 bytes). The worker composes the handoff before continuing the scene;
/// ordinary requests then reuse this session. Reconnects omit the old handoff.
std::optional<std::uint32_t> request_scene(
    const Scenes::Scene &scene, int width, int height, int target_fps,
    const nlohmann::json &transition = nlohmann::json::object());

/// Keep the worker fed from the same authoritative runtime state as local
/// scenes. Audio is mirrored losslessly enough for any current audio scene; the
/// generic RuntimeInputs snapshot covers plugin-owned contextual data.
void publish_runtime_state(std::uint32_t session);

/// Cheap chunk-receiver gate. Does not build diagnostics or scan contracts;
/// submit_frame still validates the session again when the frame is complete.
[[nodiscard]] bool session_requested(std::uint32_t session);

/// Called by the matrix RenderOffload UDP plugin. Frames are latest-wins: stale
/// sessions and out-of-order sequences are discarded immediately.
bool submit_frame(std::uint32_t session, std::uint32_t sequence,
                  int width, int height, std::span<const std::uint8_t> rgb);

/// Copy the latest complete RGB888 frame into a canvas if it is recent enough.
/// No queue is maintained, so network jitter cannot build up latency.
bool copy_latest(std::uint32_t session, rgb_matrix::FrameCanvas *canvas,
                 int width, int height, double max_age_ms = 300.0);

/// Age of the newest received frame for `session`, without taking the state
/// lock. Empty when no frame of that session has arrived.
std::optional<double> latest_frame_age_ms(std::uint32_t session);

/// Return a start command for a newly reconnected desktop, if a session is
/// currently requested.
std::optional<nlohmann::json> reconnect_command();

Status status();
void stop();

} // namespace RemoteRender
