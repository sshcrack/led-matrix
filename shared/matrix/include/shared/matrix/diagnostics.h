#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace Diagnostics {

class RuntimeDiagnostics {
public:
    static RuntimeDiagnostics &instance();

    void set_active_scene(const std::string &scene);
    void record_render(const std::string &scene, double render_ms, int target_fps);
    void record_scene_error(const std::string &scene, const std::string &message);

    void record_udp_datagram(std::size_t bytes);
    void record_udp_packet(bool handled);
    void record_udp_malformed();

    void record_audio_packet(std::uint32_t sequence);
    void record_audio_decode_error();

    [[nodiscard]] nlohmann::json snapshot() const;

private:
    RuntimeDiagnostics();

    mutable std::mutex mutex_;
    std::uint64_t started_ms_ = 0;
    std::string active_scene_;

    std::uint64_t render_frames_ = 0;
    std::uint64_t dropped_render_frames_ = 0;
    double render_ms_ema_ = 0.0;
    double render_ms_max_ = 0.0;
    double fps_ema_ = 0.0;
    std::uint64_t last_render_ms_ = 0;
    std::unordered_map<std::string, std::uint64_t> scene_error_counts_;
    std::unordered_map<std::string, std::string> scene_last_errors_;

    std::uint64_t udp_datagrams_ = 0;
    std::uint64_t udp_bytes_ = 0;
    std::uint64_t udp_packets_ = 0;
    std::uint64_t udp_unhandled_ = 0;
    std::uint64_t udp_malformed_ = 0;

    std::uint64_t audio_packets_ = 0;
    std::uint64_t audio_sequence_gaps_ = 0;
    std::uint64_t audio_decode_errors_ = 0;
    std::uint32_t last_audio_sequence_ = 0;
    bool have_audio_sequence_ = false;
};

} // namespace Diagnostics
