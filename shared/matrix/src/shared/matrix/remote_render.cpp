#include "shared/matrix/remote_render.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "led-matrix.h"
#include "shared/common/remote_render_protocol.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/audio_state.h"
#include "shared/matrix/media_artwork_state.h"
#include "shared/matrix/runtime_inputs.h"

namespace RemoteRender {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto WorkerHeartbeatTimeout = std::chrono::milliseconds(2800);

struct State {
    std::mutex mutex;
    CommandSender sender;
    std::uint32_t next_session = 1;
    std::uint32_t session = 0;
    std::uint32_t last_sequence = 0;
    std::string scene;
    std::string scene_uuid;
    int width = 0;
    int height = 0;
    int target_fps = 60;
    nlohmann::json start_command;
    std::vector<std::uint8_t> frame;
    Clock::time_point frame_time{};
    Clock::time_point last_state_publish{};
    Clock::time_point worker_heartbeat{};
    std::unordered_set<std::string> worker_scenes;
};

State &state()
{
    static State value;
    return value;
}

double age_ms(Clock::time_point then)
{
    if (then == Clock::time_point{})
        return 1.0e9;
    return std::chrono::duration<double, std::milli>(Clock::now() - then).count();
}

bool worker_alive(const State &s)
{
    return s.worker_heartbeat != Clock::time_point{}
        && Clock::now() - s.worker_heartbeat <= WorkerHeartbeatTimeout;
}

bool worker_supports(const State &s, std::string_view scene)
{
    if (!worker_alive(s))
        return false;
    return scene.empty() || s.worker_scenes.contains(std::string(scene));
}

nlohmann::json audio_snapshot_json()
{
    const auto audio = AudioState::snapshot();
    return {
        {"available", audio.available},
        {"fresh", audio.fresh()},
        {"sequence", audio.sequence},
        {"timestamp_ms", audio.timestamp_ms},
        {"flags", audio.flags},
        {"beat_counter", audio.beat_counter},
        {"onset_counter", audio.onset_counter},
        {"drop_counter", audio.drop_counter},
        {"section_counter", audio.section_counter},
        {"features", audio.features},
        {"spectrum", audio.spectrum},
        {"waveform", audio.waveform},
    };
}

std::uint32_t allocate_session(State &s)
{
    auto result = s.next_session++;
    if (result == 0)
        result = s.next_session++;
    return result;
}
} // namespace

void set_command_sender(CommandSender sender)
{
    std::lock_guard lock(state().mutex);
    state().sender = std::move(sender);
}

void clear_command_sender()
{
    std::lock_guard lock(state().mutex);
    state().sender = {};
}

void report_worker_heartbeat(int protocol_version, const std::vector<std::string> &scenes)
{
    if (protocol_version != RemoteRenderProtocol::Version)
        return;
    auto &s = state();
    std::lock_guard lock(s.mutex);
    s.worker_heartbeat = Clock::now();
    s.worker_scenes.clear();
    s.worker_scenes.reserve(scenes.size());
    for (const auto &scene : scenes)
        if (!scene.empty()) s.worker_scenes.insert(scene);
}

bool worker_available(std::string_view scene)
{
    auto &s = state();
    std::lock_guard lock(s.mutex);
    return worker_supports(s, scene);
}

std::optional<std::uint32_t> request_scene(
    const Scenes::Scene &scene, int width, int height, int target_fps)
{
    const auto caps = scene.get_capabilities();
    if (!caps.supports_remote_rendering)
        return std::nullopt;

    const auto input_snapshot = RuntimeInputs::to_json(RuntimeInputs::snapshot());
    const auto audio_snapshot = audio_snapshot_json();
    CommandSender sender;
    nlohmann::json command;
    std::uint32_t session = 0;
    {
        auto &s = state();
        std::lock_guard lock(s.mutex);
        if (!s.sender || !worker_supports(s, scene.get_name()))
            return std::nullopt;

        const auto uuid = scene.get_uuid();
        const auto arguments = scene.to_json();
        if (s.session != 0 && s.scene == scene.get_name() && s.scene_uuid == uuid
            && s.width == width && s.height == height
            && s.start_command.value("arguments", nlohmann::json::object()) == arguments) {
            return s.session;
        }

        s.session = allocate_session(s);
        s.last_sequence = 0;
        s.scene = scene.get_name();
        s.scene_uuid = uuid;
        s.width = width;
        s.height = height;
        s.target_fps = std::clamp(target_fps, 1, 60);
        s.frame.clear();
        s.frame_time = {};
        s.last_state_publish = {};
        s.start_command = {
            {"op", "start"},
            {"protocol", RemoteRenderProtocol::Version},
            {"session", s.session},
            {"scene", s.scene},
            {"uuid", uuid},
            {"width", s.width},
            {"height", s.height},
            {"target_fps", s.target_fps},
            {"variant", scene.get_variant_id()},
            {"arguments", arguments},
            {"runtime_state", scene.snapshot_runtime_state()},
            {"elapsed_seconds", scene.get_frame_context().elapsed_seconds},
            {"inputs", input_snapshot},
            {"audio", audio_snapshot},
            {"artwork", MediaArtworkState::to_json(MediaArtworkState::snapshot())},
        };
        session = s.session;
        command = s.start_command;
        sender = s.sender;
    }

    sender(command);
    return session;
}

void publish_runtime_state(std::uint32_t session)
{
    CommandSender sender;
    {
        auto &s = state();
        std::lock_guard lock(s.mutex);
        if (!s.sender || session == 0 || session != s.session)
            return;
        const auto now = Clock::now();
        if (s.last_state_publish != Clock::time_point{}
            && now - s.last_state_publish < std::chrono::milliseconds(40))
            return;
        s.last_state_publish = now;
        sender = s.sender;
    }

    sender({
        {"op", "state"},
        {"protocol", RemoteRenderProtocol::Version},
        {"session", session},
        {"audio", audio_snapshot_json()},
        {"inputs", RuntimeInputs::to_json(RuntimeInputs::snapshot())},
        {"artwork", MediaArtworkState::to_json(MediaArtworkState::snapshot())},
    });
}

bool submit_frame(std::uint32_t session, std::uint32_t sequence,
                  int width, int height, std::span<const std::uint8_t> rgb)
{
    auto &s = state();
    std::lock_guard lock(s.mutex);
    if (session == 0 || session != s.session || width != s.width || height != s.height)
        return false;
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    if (rgb.size() != expected)
        return false;
    if (s.last_sequence != 0 && sequence <= s.last_sequence)
        return false;

    s.last_sequence = sequence;
    s.frame.assign(rgb.begin(), rgb.end());
    s.frame_time = Clock::now();
    return true;
}

bool copy_latest(std::uint32_t session, rgb_matrix::FrameCanvas *canvas,
                 int width, int height, double max_age_ms)
{
    if (!canvas)
        return false;

    std::vector<std::uint8_t> frame;
    {
        auto &s = state();
        std::lock_guard lock(s.mutex);
        if (session == 0 || session != s.session || width != s.width || height != s.height
            || s.frame.empty() || age_ms(s.frame_time) > max_age_ms)
            return false;
        frame = s.frame;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 3);
            canvas->SetPixel(x, y, frame[offset], frame[offset + 1], frame[offset + 2]);
        }
    }
    return true;
}

std::optional<nlohmann::json> reconnect_command()
{
    std::lock_guard lock(state().mutex);
    if (state().session == 0 || state().start_command.empty())
        return std::nullopt;
    return state().start_command;
}

Status status()
{
    auto &s = state();
    std::lock_guard lock(s.mutex);
    Status result;
    result.requested = s.session != 0;
    result.frame_fresh = result.requested && !s.frame.empty() && age_ms(s.frame_time) <= 300.0;
    result.session = s.session;
    result.last_sequence = s.last_sequence;
    result.frame_age_ms = age_ms(s.frame_time);
    result.scene = s.scene;
    result.worker_available = worker_alive(s);
    result.worker_scene_count = s.worker_scenes.size();
    return result;
}

void stop()
{
    CommandSender sender;
    nlohmann::json command;
    {
        auto &s = state();
        std::lock_guard lock(s.mutex);
        if (s.session == 0)
            return;
        command = {{"op", "stop"}, {"protocol", RemoteRenderProtocol::Version}, {"session", s.session}};
        sender = s.sender;
        s.session = 0;
        s.last_sequence = 0;
        s.scene.clear();
        s.scene_uuid.clear();
        s.frame.clear();
        s.frame_time = {};
        s.start_command = {};
    }
    if (sender)
        sender(command);
}

} // namespace RemoteRender
