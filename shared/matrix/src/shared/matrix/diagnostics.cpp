#include <shared/matrix/diagnostics.h>

#include <algorithm>
#include <chrono>
#include <vector>

namespace {
std::uint64_t monotonic_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

namespace Diagnostics {
RuntimeDiagnostics::RuntimeDiagnostics() : started_ms_(monotonic_ms()) {}

RuntimeDiagnostics &RuntimeDiagnostics::instance() {
    static RuntimeDiagnostics diagnostics;
    return diagnostics;
}

void RuntimeDiagnostics::set_active_scene(const std::string &scene) {
    std::lock_guard lock(mutex_);
    active_scene_ = scene;
}

void RuntimeDiagnostics::record_render(const std::string &scene, double render_ms, int target_fps, float quality_scale) {
    std::lock_guard lock(mutex_);
    active_scene_ = scene;
    ++render_frames_;
    render_ms_max_ = std::max(render_ms_max_, render_ms);
    render_ms_ema_ = render_frames_ == 1 ? render_ms : render_ms_ema_ * 0.94 + render_ms * 0.06;

    const auto now = monotonic_ms();
    if (last_render_ms_ != 0 && now > last_render_ms_) {
        const double instantaneous_fps = 1000.0 / static_cast<double>(now - last_render_ms_);
        fps_ema_ = fps_ema_ <= 0.0 ? instantaneous_fps : fps_ema_ * 0.92 + instantaneous_fps * 0.08;
    }
    last_render_ms_ = now;

    const double budget_ms = 1000.0 / static_cast<double>(std::max(1, target_fps));
    const bool slow = render_ms > budget_ms * 1.15;
    if (slow) ++dropped_render_frames_;

    auto &stats = scene_render_stats_[scene];
    ++stats.frames;
    if (slow) ++stats.slow_frames;
    stats.render_ms_max = std::max(stats.render_ms_max, render_ms);
    stats.render_ms_ema = stats.frames == 1 ? render_ms : stats.render_ms_ema * 0.92 + render_ms * 0.08;
    stats.quality_scale = quality_scale;
    stats.target_fps = std::max(1, target_fps);
    stats.recent_ms[stats.recent_next] = render_ms;
    stats.recent_next = (stats.recent_next + 1) % stats.recent_ms.size();
    stats.recent_count = std::min(stats.recent_count + 1, stats.recent_ms.size());
}

void RuntimeDiagnostics::record_scene_error(const std::string &scene, const std::string &message) {
    std::lock_guard lock(mutex_);
    ++scene_error_counts_[scene];
    scene_last_errors_[scene] = message;
}

void RuntimeDiagnostics::record_udp_datagram(std::size_t bytes) {
    std::lock_guard lock(mutex_);
    ++udp_datagrams_;
    udp_bytes_ += bytes;
}

void RuntimeDiagnostics::record_udp_packet(bool handled) {
    std::lock_guard lock(mutex_);
    ++udp_packets_;
    if (!handled) ++udp_unhandled_;
}

void RuntimeDiagnostics::record_udp_malformed() {
    std::lock_guard lock(mutex_);
    ++udp_malformed_;
}

void RuntimeDiagnostics::record_audio_packet(std::uint32_t sequence) {
    std::lock_guard lock(mutex_);
    ++audio_packets_;
    if (have_audio_sequence_ && sequence > last_audio_sequence_ + 1)
        audio_sequence_gaps_ += static_cast<std::uint64_t>(sequence - last_audio_sequence_ - 1);
    last_audio_sequence_ = sequence;
    have_audio_sequence_ = true;
}

void RuntimeDiagnostics::record_audio_decode_error() {
    std::lock_guard lock(mutex_);
    ++audio_decode_errors_;
}

void RuntimeDiagnostics::set_director_state(nlohmann::json state) {
    std::lock_guard lock(mutex_);
    director_state_ = std::move(state);
}

void RuntimeDiagnostics::set_transition_state(nlohmann::json state) {
    std::lock_guard lock(mutex_);
    transition_state_ = std::move(state);
}

void RuntimeDiagnostics::set_render_placement(nlohmann::json state) {
    std::lock_guard lock(mutex_);
    render_placement_state_ = std::move(state);
}

std::optional<double> RuntimeDiagnostics::scene_render_p95(const std::string &scene) const {
    std::lock_guard lock(mutex_);
    const auto it = scene_render_stats_.find(scene);
    if (it == scene_render_stats_.end() || it->second.recent_count < 4)
        return std::nullopt;
    const auto &stats = it->second;
    std::vector<double> samples(stats.recent_ms.begin(), stats.recent_ms.begin() + stats.recent_count);
    std::sort(samples.begin(), samples.end());
    const std::size_t index = std::min(samples.size() - 1,
        static_cast<std::size_t>(std::floor((samples.size() - 1) * 0.95)));
    return samples[index];
}

nlohmann::json RuntimeDiagnostics::snapshot() const {
    std::lock_guard lock(mutex_);
    const auto now = monotonic_ms();
    const double uptime_seconds = std::max(0.001, static_cast<double>(now - started_ms_) / 1000.0);

    nlohmann::json errors = nlohmann::json::object();
    for (const auto &[scene, count] : scene_error_counts_) {
        errors[scene] = {
            {"count", count},
            {"last_error", scene_last_errors_.contains(scene) ? scene_last_errors_.at(scene) : std::string{}}
        };
    }

    nlohmann::json scene_performance = nlohmann::json::object();
    for (const auto &[scene, stats] : scene_render_stats_) {
        std::vector<double> samples(stats.recent_ms.begin(), stats.recent_ms.begin() + stats.recent_count);
        std::sort(samples.begin(), samples.end());
        auto percentile = [&](double p) {
            if (samples.empty()) return 0.0;
            const std::size_t index = std::min(samples.size() - 1,
                static_cast<std::size_t>(std::floor((samples.size() - 1) * p)));
            return samples[index];
        };
        const double frame_budget_ms = 1000.0 / static_cast<double>(std::max(1, stats.target_fps));
        const double p95 = percentile(0.95);
        scene_performance[scene] = {
            {"frames", stats.frames},
            {"target_fps", stats.target_fps},
            {"frame_budget_ms", frame_budget_ms},
            {"render_ms_average", stats.render_ms_ema},
            {"render_ms_p50", percentile(0.50)},
            {"render_ms_p95", p95},
            {"render_ms_p99", percentile(0.99)},
            {"render_ms_max", stats.render_ms_max},
            {"p95_budget_ratio", p95 / frame_budget_ms},
            {"offload_pressure", p95 > frame_budget_ms * 0.72},
            {"slow_frames", stats.slow_frames},
            {"quality_scale", stats.quality_scale}
        };
    }

    return {
        {"uptime_seconds", uptime_seconds},
        {"renderer", {
            {"active_scene", active_scene_},
            {"frames", render_frames_},
            {"fps", fps_ema_},
            {"render_ms_average", render_ms_ema_},
            {"render_ms_max", render_ms_max_},
            {"slow_frames", dropped_render_frames_},
            {"scene_errors", errors},
            {"scene_performance", scene_performance}
        }},
        {"udp", {
            {"datagrams", udp_datagrams_},
            {"packets", udp_packets_},
            {"bytes", udp_bytes_},
            {"datagrams_per_second", static_cast<double>(udp_datagrams_) / uptime_seconds},
            {"bytes_per_second", static_cast<double>(udp_bytes_) / uptime_seconds},
            {"malformed", udp_malformed_},
            {"unhandled", udp_unhandled_}
        }},
        {"audio_transport", {
            {"packets", audio_packets_},
            {"sequence_gaps", audio_sequence_gaps_},
            {"decode_errors", audio_decode_errors_},
            {"last_sequence", have_audio_sequence_ ? nlohmann::json(last_audio_sequence_) : nlohmann::json(nullptr)}
        }},
        {"director", director_state_},
        {"transition", transition_state_},
        {"render_placement", render_placement_state_}
    };
}
}
