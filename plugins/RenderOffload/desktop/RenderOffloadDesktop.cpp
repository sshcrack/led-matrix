#include "RenderOffloadDesktop.h"

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "RenderFramePacket.h"

REGISTER_PLUGIN(RenderOffload, RenderOffloadDesktop)

namespace {
template <typename T>
T value_or(const nlohmann::json &j, const char *key, T fallback)
{
    if (!j.contains(key))
        return fallback;
    try { return j.at(key).get<T>(); }
    catch (...) { return fallback; }
}
} // namespace

void RenderOffloadDesktop::initialize_imgui(
    ImGuiContext *context, ImGuiMemAllocFunc *alloc_fn,
    ImGuiMemFreeFunc *free_fn, void **user_data)
{
    ImGui::SetCurrentContext(context);
    ImGui::GetAllocatorFunctions(alloc_fn, free_fn, user_data);
}

void RenderOffloadDesktop::render()
{
    std::lock_guard lock(mutex_);
    ImGui::TextUnformatted("Automatic render offload");
    ImGui::Text("State: %s", active_ ? "rendering on desktop" : "idle");
    if (active_) {
        ImGui::Text("Renderer: %s", renderer_id_.c_str());
        ImGui::Text("Target: %dx%d @ %d FPS", metaball_params_.width, metaball_params_.height, target_fps_);
        ImGui::Text("Session: %u", session_);
    }
    ImGui::TextWrapped("The Pi requests this automatically only when a scene exceeds its measured frame budget.");
}

void RenderOffloadDesktop::on_websocket_message(std::string message)
{
    try {
        const auto command = nlohmann::json::parse(message);
        const auto op = value_or<std::string>(command, "op", "");
        const auto session = value_or<std::uint32_t>(command, "session", 0);
        if (session == 0)
            return;

        std::lock_guard lock(mutex_);
        if (op == "start") {
            const auto renderer = value_or<std::string>(command, "renderer", "");
            if (renderer != "metablob") {
                spdlog::warn("RenderOffload does not support desktop renderer '{}'", renderer);
                return;
            }

            session_ = session;
            sequence_ = 0;
            renderer_id_ = renderer;
            target_fps_ = std::clamp(value_or<int>(command, "target_fps", 60), 1, 60);
            metaball_params_.width = std::clamp(value_or<int>(command, "width", 128), 1, 512);
            metaball_params_.height = std::clamp(value_or<int>(command, "height", 128), 1, 512);

            const auto arguments = command.value("arguments", nlohmann::json::object());
            metaball_params_.blob_count = std::clamp(value_or<int>(arguments, "num_blobs", 10), 1, 24);
            metaball_params_.threshold = value_or<float>(arguments, "threshold", 0.0003f);
            metaball_params_.speed = value_or<float>(arguments, "speed", 0.25f);
            metaball_params_.move_range = value_or<float>(arguments, "move_range", 0.5f);
            metaball_params_.color_speed = value_or<float>(arguments, "color_speed", 0.033f);
            metaball_params_.audio_reactive = value_or<bool>(arguments, "audio_reactive", false);
            metaball_params_.audio_strength = value_or<float>(arguments, "audio_strength", 0.75f);

            metaball_audio_ = {};
            time_seconds_ = 0.0f;
            last_render_ = {};
            audio_fresh_ = false;
            audio_bass_target_ = audio_mids_target_ = audio_treble_target_ = audio_balance_target_ = 0.0f;
            audio_kick_ = 0.0f;
            beat_counter_ = drop_counter_ = section_counter_ = 0;
            active_ = true;
            spdlog::debug("Desktop render offload started session {} for {}", session_, renderer_id_);
            return;
        }

        if (session != session_)
            return;

        if (op == "stop") {
            active_ = false;
            renderer_id_.clear();
            return;
        }

        if (op == "audio") {
            audio_fresh_ = value_or<bool>(command, "fresh", false);
            audio_bass_target_ = value_or<float>(command, "bass", 0.0f);
            audio_mids_target_ = value_or<float>(command, "mids", 0.0f);
            audio_treble_target_ = value_or<float>(command, "treble", 0.0f);
            audio_balance_target_ = value_or<float>(command, "balance", 0.0f);
            audio_kick_ = value_or<float>(command, "kick", 0.0f);

            const auto beat = value_or<std::uint64_t>(command, "beat_counter", beat_counter_);
            const auto drop = value_or<std::uint64_t>(command, "drop_counter", drop_counter_);
            const auto section = value_or<std::uint64_t>(command, "section_counter", section_counter_);
            if (audio_fresh_ && beat != beat_counter_)
                metaball_audio_.beat_pulse = std::max(metaball_audio_.beat_pulse, 0.55f + audio_kick_ * 0.45f);
            if (audio_fresh_ && drop != drop_counter_)
                metaball_audio_.drop_pulse = 1.0f;
            if (audio_fresh_ && section != section_counter_)
                metaball_audio_.section_hue += 0.17f;
            beat_counter_ = beat;
            drop_counter_ = drop;
            section_counter_ = section;
        }
    } catch (const std::exception &e) {
        spdlog::warn("Rejected RenderOffload command: {}", e.what());
    }
}

std::vector<std::unique_ptr<UdpPacket>>
RenderOffloadDesktop::compute_next_packets(const std::string &)
{
    std::lock_guard lock(mutex_);
    if (!active_ || session_ == 0 || renderer_id_ != "metablob")
        return {};

    const auto now = Clock::now();
    const auto min_interval = std::chrono::duration<double>(1.0 / static_cast<double>(target_fps_));
    if (last_render_ != Clock::time_point{} && now - last_render_ < min_interval)
        return {};

    float dt = 1.0f / static_cast<float>(target_fps_);
    if (last_render_ != Clock::time_point{})
        dt = static_cast<float>(std::chrono::duration<double>(now - last_render_).count());
    dt = std::clamp(dt, 0.0f, 0.10f);
    last_render_ = now;

    const float response = 1.0f - std::exp(-dt * 8.0f);
    const float bass = audio_fresh_ ? audio_bass_target_ : 0.0f;
    const float mids = audio_fresh_ ? audio_mids_target_ : 0.0f;
    const float treble = audio_fresh_ ? audio_treble_target_ : 0.0f;
    const float balance = audio_fresh_ ? audio_balance_target_ : 0.0f;
    metaball_audio_.bass += (bass - metaball_audio_.bass) * response;
    metaball_audio_.mids += (mids - metaball_audio_.mids) * response;
    metaball_audio_.treble += (treble - metaball_audio_.treble) * response;
    metaball_audio_.balance += (balance - metaball_audio_.balance) * response;
    metaball_audio_.beat_pulse = std::max(0.0f, metaball_audio_.beat_pulse - dt * 3.0f);
    metaball_audio_.drop_pulse = std::max(0.0f, metaball_audio_.drop_pulse - dt * 0.95f);

    const auto &rgb = metaball_renderer_.render(
        metaball_params_, metaball_audio_, time_seconds_, 1.0f);
    time_seconds_ += dt;

    constexpr std::size_t ChunkBytes = 1200;
    const auto frame_sequence = ++sequence_;
    const auto chunk_count = static_cast<std::uint16_t>((rgb.size() + ChunkBytes - 1) / ChunkBytes);
    std::vector<std::unique_ptr<UdpPacket>> packets;
    packets.reserve(chunk_count);
    for (std::uint16_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
        const std::size_t offset = static_cast<std::size_t>(chunk_index) * ChunkBytes;
        const std::size_t count = std::min(ChunkBytes, rgb.size() - offset);
        std::vector<std::uint8_t> chunk(rgb.begin() + static_cast<std::ptrdiff_t>(offset),
                                        rgb.begin() + static_cast<std::ptrdiff_t>(offset + count));
        packets.push_back(std::make_unique<RenderFramePacket>(
            session_, frame_sequence,
            static_cast<std::uint16_t>(metaball_params_.width),
            static_cast<std::uint16_t>(metaball_params_.height),
            chunk_index, chunk_count, static_cast<std::uint32_t>(offset),
            std::move(chunk)));
    }
    return packets;
}
