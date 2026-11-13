#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace Diagnostics {

class RuntimeDiagnostics {
public:
    static RuntimeDiagnostics &instance();

    void set_active_scene(const std::string &scene);
    void record_render(const std::string &scene, double render_ms, int target_fps, float quality_scale);
    /// Record a frame that actually reached the matrix. This path is lock-free
    /// so diagnostics can never delay the hardware refresh loop.
    void record_presentation(int target_fps);
    void reset_presentation_cadence();
    void record_scene_error(const std::string &scene, const std::string &message);

    void record_udp_datagram(std::size_t bytes);
    void record_udp_packet(bool handled);
    void record_udp_malformed();

    void record_audio_packet(std::uint32_t sequence);
    void record_audio_decode_error();

    void set_director_state(nlohmann::json state);
    void set_transition_state(nlohmann::json state);

    /// Transition timeline (render thread only). `begin` is called when the
    /// outgoing scene stops presenting; `mark_composing` once setup that does
    /// not present (director choice, initialisation, optional beat hold) ends;
    /// `end` once the transition phase returns. Presentations in between are
    /// measured as transition frames, the first presentation afterwards as the
    /// handoff gap. The result is merged into `transition` and appended to
    /// `transition_history` so stutters can be compared across transitions.
    void begin_transition_timeline();
    void mark_transition_composing();
    void end_transition_timeline();
    void cancel_transition_timeline();
    void set_render_placement(nlohmann::json state);
    void set_hardware_state(nlohmann::json state);
    [[nodiscard]] std::optional<double> scene_render_p95(const std::string &scene) const;
    [[nodiscard]] std::uint64_t scene_error_count(const std::string &scene) const;

    [[nodiscard]] nlohmann::json snapshot() const;

private:
    RuntimeDiagnostics();

    mutable std::mutex mutex_;
    std::uint64_t started_ms_ = 0;
    std::string active_scene_;

    std::uint64_t render_frames_ = 0;
    std::uint64_t dropped_render_frames_ = 0;
    std::atomic<std::uint64_t> render_samples_skipped_{0};

    std::atomic<std::uint64_t> presentation_frames_{0};
    std::atomic<std::uint64_t> presentation_intervals_{0};
    std::atomic<std::uint64_t> presentation_late_frames_{0};
    std::atomic<std::uint64_t> presentation_estimated_missed_refreshes_{0};
    std::atomic<std::uint64_t> presentation_last_us_{0};
    std::atomic<std::uint64_t> presentation_last_interval_us_{0};
    std::atomic<std::uint64_t> presentation_interval_total_us_{0};
    std::atomic<std::uint64_t> presentation_interval_max_us_{0};
    double render_ms_ema_ = 0.0;
    double render_ms_max_ = 0.0;
    double fps_ema_ = 0.0;
    std::uint64_t last_render_ms_ = 0;
    struct SceneRenderStats {
        std::uint64_t frames = 0;
        std::uint64_t slow_frames = 0;
        double render_ms_ema = 0.0;
        double render_ms_max = 0.0;
        float quality_scale = 1.0f;
        int target_fps = 60;
        std::array<double, 128> recent_ms{};
        std::size_t recent_count = 0;
        std::size_t recent_next = 0;
    };
    std::unordered_map<std::string, SceneRenderStats> scene_render_stats_;

    std::unordered_map<std::string, std::uint64_t> scene_error_counts_;
    std::unordered_map<std::string, std::string> scene_last_errors_;

    // UDP telemetry is deliberately independent of mutex_. Desktop frame
    // traffic can be high-rate and must not contend with render diagnostics.
    std::atomic<std::uint64_t> udp_datagrams_{0};
    std::atomic<std::uint64_t> udp_bytes_{0};
    std::atomic<std::uint64_t> udp_packets_{0};
    std::atomic<std::uint64_t> udp_unhandled_{0};
    std::atomic<std::uint64_t> udp_malformed_{0};

    std::uint64_t audio_packets_ = 0;
    std::uint64_t audio_sequence_gaps_ = 0;
    std::uint64_t audio_decode_errors_ = 0;
    std::uint32_t last_audio_sequence_ = 0;
    bool have_audio_sequence_ = false;
    nlohmann::json director_state_ = nlohmann::json::object();
    nlohmann::json transition_state_ = nlohmann::json::object();
    std::deque<nlohmann::json> transition_history_;

    // Written and read only by the render thread (record_presentation and the
    // timeline calls), so no synchronisation is needed for these fields.
    struct TransitionTimeline {
        bool active = false;
        bool awaiting_handoff = false;
        std::uint64_t last_presented_us = 0;   // any presentation, never reset
        std::uint64_t outgoing_last_us = 0;    // last frame before the transition
        std::uint64_t begin_us = 0;
        std::uint64_t composing_us = 0;
        std::uint64_t first_frame_us = 0;
        std::uint64_t last_frame_us = 0;
        std::uint64_t frames = 0;
        std::vector<std::uint32_t> intervals_us;
    } timeline_;
    nlohmann::json render_placement_state_ = nlohmann::json::object();
    nlohmann::json hardware_state_ = nlohmann::json::object();
};

} // namespace Diagnostics
