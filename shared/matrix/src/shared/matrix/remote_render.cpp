#include "shared/matrix/remote_render.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
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

struct FrameSnapshot {
    std::uint32_t session = 0;
    std::uint32_t sequence = 0;
    int width = 0;
    int height = 0;
    Clock::time_point received_at{};
    std::vector<std::uint8_t> rgb;
};

struct WorkerSnapshot {
    Clock::time_point heartbeat{};
    std::unordered_set<std::string> scenes;
};

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
    std::atomic<std::shared_ptr<const FrameSnapshot>> latest_frame{};
    Clock::time_point last_state_publish{};
    std::atomic<std::shared_ptr<const WorkerSnapshot>> latest_worker{};
    bool state_publish_pending = false;
    std::uint32_t pending_state_session = 0;
    std::condition_variable publisher_cv;
    std::jthread publisher_thread;
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

bool worker_alive(const std::shared_ptr<const WorkerSnapshot> &worker)
{
    return worker && worker->heartbeat != Clock::time_point{}
        && Clock::now() - worker->heartbeat <= WorkerHeartbeatTimeout;
}

bool worker_supports(const State &s, std::string_view scene)
{
    const auto worker = s.latest_worker.load(std::memory_order_acquire);
    if (!worker_alive(worker))
        return false;
    return scene.empty() || worker->scenes.contains(std::string(scene));
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

void ensure_state_publisher(State &s)
{
    if (s.publisher_thread.joinable())
        return;

    s.publisher_thread = std::jthread([&s](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::uint32_t session = 0;
            CommandSender sender;
            {
                std::unique_lock lock(s.mutex);
                s.publisher_cv.wait_for(lock, std::chrono::milliseconds(100), [&] {
                    return stop.stop_requested() || s.state_publish_pending;
                });
                if (stop.stop_requested())
                    return;
                if (!s.state_publish_pending)
                    continue;

                session = s.pending_state_session;
                s.state_publish_pending = false;
                if (session == 0 || session != s.session || !s.sender)
                    continue;
                sender = s.sender;
            }

            // Snapshot/JSON construction and WebSocket delivery deliberately
            // happen here, never on the matrix render thread.
            auto command = nlohmann::json{
                {"op", "state"},
                {"protocol", RemoteRenderProtocol::Version},
                {"session", session},
                {"audio", audio_snapshot_json()},
                {"inputs", RuntimeInputs::to_json(RuntimeInputs::snapshot())},
                {"artwork", MediaArtworkState::to_json(MediaArtworkState::snapshot())},
            };

            {
                std::lock_guard lock(s.mutex);
                if (session != s.session || !s.sender)
                    continue;
            }
            sender(command);
        }
    });
}
} // namespace

void set_command_sender(CommandSender sender)
{
    auto &s = state();
    std::lock_guard lock(s.mutex);
    s.sender = std::move(sender);
    ensure_state_publisher(s);
}

void clear_command_sender()
{
    auto &s = state();
    std::lock_guard lock(s.mutex);
    s.sender = {};
    s.state_publish_pending = false;
    s.pending_state_session = 0;
}

void report_worker_heartbeat(int protocol_version, const std::vector<std::string> &scenes)
{
    if (protocol_version != RemoteRenderProtocol::Version)
        return;
    auto worker = std::make_shared<WorkerSnapshot>();
    worker->heartbeat = Clock::now();
    worker->scenes.reserve(scenes.size());
    for (const auto &scene : scenes)
        if (!scene.empty()) worker->scenes.insert(scene);
    state().latest_worker.store(std::move(worker), std::memory_order_release);
}

bool worker_available(std::string_view scene)
{
    return worker_supports(state(), scene);
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
        s.latest_frame.store({}, std::memory_order_release);
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
    auto &s = state();
    std::unique_lock lock(s.mutex, std::try_to_lock);
    if (!lock.owns_lock() || !s.sender || session == 0 || session != s.session)
        return;

    const auto now = Clock::now();
    if (s.last_state_publish != Clock::time_point{}
        && now - s.last_state_publish < std::chrono::milliseconds(40))
        return;

    s.last_state_publish = now;
    s.pending_state_session = session;
    s.state_publish_pending = true;
    s.publisher_cv.notify_one();
}

bool submit_frame(std::uint32_t session, std::uint32_t sequence,
                  int width, int height, std::span<const std::uint8_t> rgb)
{
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U;
    if (rgb.size() != expected)
        return false;

    auto next = std::make_shared<FrameSnapshot>();
    next->session = session;
    next->sequence = sequence;
    next->width = width;
    next->height = height;
    next->received_at = Clock::now();
    next->rgb.assign(rgb.begin(), rgb.end());

    auto &s = state();
    {
        std::lock_guard lock(s.mutex);
        if (session == 0 || session != s.session || width != s.width || height != s.height)
            return false;
        if (s.last_sequence != 0 && sequence <= s.last_sequence)
            return false;
        s.last_sequence = sequence;
        s.latest_frame.store(std::move(next), std::memory_order_release);
    }
    return true;
}

bool copy_latest(std::uint32_t session, rgb_matrix::FrameCanvas *canvas,
                 int width, int height, double max_age_ms)
{
    if (!canvas)
        return false;

    // The UDP receiver publishes immutable frame snapshots atomically. The
    // hardware/render thread never waits for the receiver and never copies the
    // whole RGB vector before presenting it.
    const auto frame = state().latest_frame.load(std::memory_order_acquire);
    if (!frame || session == 0 || frame->session != session
        || frame->width != width || frame->height != height
        || age_ms(frame->received_at) > max_age_ms)
        return false;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto offset = static_cast<std::size_t>((y * width + x) * 3);
            canvas->SetPixel(x, y, frame->rgb[offset], frame->rgb[offset + 1], frame->rgb[offset + 2]);
        }
    }
    return true;
}

std::optional<double> latest_frame_age_ms(std::uint32_t session)
{
    const auto frame = state().latest_frame.load(std::memory_order_acquire);
    if (!frame || session == 0 || frame->session != session)
        return std::nullopt;
    return age_ms(frame->received_at);
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
    const auto frame = s.latest_frame.load(std::memory_order_acquire);
    result.frame_fresh = result.requested && frame && frame->session == s.session
        && age_ms(frame->received_at) <= 300.0;
    result.session = s.session;
    result.last_sequence = s.last_sequence;
    result.frame_age_ms = frame && frame->session == s.session ? age_ms(frame->received_at) : 1.0e9;
    result.scene = s.scene;
    const auto worker = s.latest_worker.load(std::memory_order_acquire);
    result.worker_available = worker_alive(worker);
    result.worker_scene_count = worker ? worker->scenes.size() : 0;
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
        s.latest_frame.store({}, std::memory_order_release);
        s.start_command = {};
        s.state_publish_pending = false;
        s.pending_state_session = 0;
    }
    if (sender)
        sender(command);
}

} // namespace RemoteRender
