#include "shared/matrix/remote_render.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

#include "led-matrix.h"
#include "shared/matrix/Scene.h"
#include "shared/matrix/audio_state.h"

namespace RemoteRender {
namespace {
using Clock = std::chrono::steady_clock;

struct State {
    std::mutex mutex;
    CommandSender sender;
    std::uint32_t next_session = 1;
    std::uint32_t session = 0;
    std::uint32_t last_sequence = 0;
    std::string scene;
    std::string scene_uuid;
    std::string renderer;
    int width = 0;
    int height = 0;
    int target_fps = 60;
    nlohmann::json start_command;
    std::vector<std::uint8_t> frame;
    Clock::time_point frame_time{};
    Clock::time_point last_audio_publish{};
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

std::optional<std::uint32_t> request_scene(
    const Scenes::Scene &scene, int width, int height, int target_fps)
{
    const auto caps = scene.get_capabilities();
    if (!caps.supports_remote_rendering || caps.remote_renderer.empty())
        return std::nullopt;

    CommandSender sender;
    nlohmann::json command;
    std::uint32_t session = 0;
    {
        auto &s = state();
        std::lock_guard lock(s.mutex);
        if (!s.sender)
            return std::nullopt;

        const auto uuid = scene.get_uuid();
        const auto arguments = scene.to_json();
        if (s.session != 0 && s.scene == scene.get_name() && s.scene_uuid == uuid
            && s.renderer == caps.remote_renderer && s.width == width && s.height == height
            && s.start_command.value("arguments", nlohmann::json::object()) == arguments) {
            return s.session;
        }

        s.session = allocate_session(s);
        s.last_sequence = 0;
        s.scene = scene.get_name();
        s.scene_uuid = uuid;
        s.renderer = caps.remote_renderer;
        s.width = width;
        s.height = height;
        s.target_fps = std::clamp(target_fps, 1, 60);
        s.frame.clear();
        s.frame_time = {};
        s.last_audio_publish = {};
        s.start_command = {
            {"op", "start"},
            {"session", s.session},
            {"scene", s.scene},
            {"renderer", s.renderer},
            {"width", s.width},
            {"height", s.height},
            {"target_fps", s.target_fps},
            {"variant", scene.get_variant_id()},
            {"arguments", arguments},
        };
        session = s.session;
        command = s.start_command;
        sender = s.sender;
    }

    sender(command);
    return session;
}

void publish_audio(std::uint32_t session)
{
    CommandSender sender;
    nlohmann::json command;
    {
        auto &s = state();
        std::lock_guard lock(s.mutex);
        if (!s.sender || session == 0 || session != s.session)
            return;
        const auto now = Clock::now();
        if (s.last_audio_publish != Clock::time_point{}
            && now - s.last_audio_publish < std::chrono::milliseconds(40))
            return;
        s.last_audio_publish = now;
        sender = s.sender;
    }

    const auto audio = AudioState::snapshot();
    command = {
        {"op", "audio"},
        {"session", session},
        {"fresh", audio.fresh()},
        {"bass", 0.5f * (audio.feature(AudioProtocol::Feature::SubBass)
                         + audio.feature(AudioProtocol::Feature::Bass))},
        {"mids", (audio.feature(AudioProtocol::Feature::LowMid)
                  + audio.feature(AudioProtocol::Feature::Mid)
                  + audio.feature(AudioProtocol::Feature::HighMid)) / 3.0f},
        {"treble", 0.5f * (audio.feature(AudioProtocol::Feature::Treble)
                           + audio.feature(AudioProtocol::Feature::Air))},
        {"balance", audio.feature(AudioProtocol::Feature::StereoBalance)},
        {"kick", audio.feature(AudioProtocol::Feature::Kick)},
        {"beat_counter", audio.beat_counter},
        {"drop_counter", audio.drop_counter},
        {"section_counter", audio.section_counter},
    };
    sender(command);
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
    result.renderer = s.renderer;
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
        command = {{"op", "stop"}, {"session", s.session}};
        sender = s.sender;
        s.session = 0;
        s.last_sequence = 0;
        s.scene.clear();
        s.scene_uuid.clear();
        s.renderer.clear();
        s.frame.clear();
        s.frame_time = {};
        s.start_command = {};
    }
    if (sender)
        sender(command);
}

} // namespace RemoteRender
