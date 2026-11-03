#include "SpotifyMVDesktop.h"
#include "shared/desktop/utils.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

long SpotifyMVDesktop::compute_video_seek(const std::string& url,
                                           long spotify_progress_ms,
                                           long spotify_duration_ms,
                                           const std::atomic<bool>* running) {
    if (spotify_duration_ms <= 0)
        return spotify_progress_ms;

    std::string video_id;
    auto vpos = url.find("v=");
    if (vpos != std::string::npos) {
        video_id = url.substr(vpos + 2);
        auto amp = video_id.find('&');
        if (amp != std::string::npos) video_id = video_id.substr(0, amp);
    } else {
        auto last_slash = url.rfind('/');
        if (last_slash != std::string::npos) {
            video_id = url.substr(last_slash + 1);
            auto qmark = video_id.find('?');
            if (qmark != std::string::npos) video_id = video_id.substr(0, qmark);
        }
    }
    if (video_id.empty()) {
        spdlog::warn("SpotifyMV: could not extract video ID from URL");
        return spotify_progress_ms;
    }

    double video_duration = 0;
    const std::string durCmd = get_ytdlp_network_command()
        + " --no-warnings --print duration \"" + url + "\"";
    const auto duration_result = run_command_capture(durCmd, running, 4096);
    if (duration_result.exit_code == -2)
        return spotify_progress_ms;
    if (duration_result.exit_code == 0 && !duration_result.output.empty()) {
        try { video_duration = std::stod(duration_result.output); } catch (...) {}
    }
    if (video_duration <= 0) {
        spdlog::warn("SpotifyMV: could not get video duration, falling back to raw seek");
        return spotify_progress_ms;
    }

    double intro_end = 0;
    double outro_start = video_duration;

    if (running && !running->load(std::memory_order_acquire))
        return spotify_progress_ms;

    {
        auto response = cpr::Get(
            cpr::Url{"https://sponsor.ajay.app/api/skipSegments"},
            cpr::Parameters{
                {"videoID", video_id},
                {"categories", R"(["intro","outro","music_offtopic"])"}
            },
            cpr::Timeout{1200}
        );

        if (response.status_code == 200 && !response.text.empty()) {
            try {
                auto segments = nlohmann::json::parse(response.text);
                for (auto& seg : segments) {
                    if (!seg.contains("segment") || !seg["segment"].is_array()
                        || seg["segment"].size() < 2)
                        continue;
                    double start = seg["segment"][0].get<double>();
                    double end = seg["segment"][1].get<double>();
                    std::string cat = seg.value("category", "");

                    if (cat == "intro" && end > intro_end)
                        intro_end = end;
                    if (cat == "outro" && start < outro_start)
                        outro_start = start;
                    if (cat == "music_offtopic") {
                        if (end > intro_end && end < video_duration / 2)
                            intro_end = end;
                        if (start < outro_start && start > video_duration / 2)
                            outro_start = start;
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("SpotifyMV: failed to parse SponsorBlock JSON: {}", e.what());
            }
        }
    }

    double spotify_dur_sec = spotify_duration_ms / 1000.0;
    if (intro_end == 0 && outro_start >= video_duration && video_duration > spotify_dur_sec) {
        double diff = video_duration - spotify_dur_sec;
        intro_end = diff * 0.35;
        outro_start = video_duration - diff * 0.65;
        spdlog::info("SpotifyMV: no SponsorBlock data, using duration heuristic (intro={:.1f}s, outro={:.1f}s)",
                     intro_end, video_duration - outro_start);
    }

    double music_duration = outro_start - intro_end;
    if (music_duration <= 0) {
        spdlog::warn("SpotifyMV: invalid music duration ({:.1f}s), falling back to raw seek", music_duration);
        return spotify_progress_ms;
    }

    double ratio = static_cast<double>(spotify_progress_ms) / spotify_duration_ms;
    double video_seek_sec = intro_end + ratio * music_duration;
    if (video_seek_sec < 0) video_seek_sec = 0;
    if (video_seek_sec > video_duration) video_seek_sec = video_duration;

    long seek_ms = static_cast<long>(video_seek_sec * 1000.0);
    spdlog::info("SpotifyMV: {}ms in song \u2192 {}ms in video (intro={:.1f}s, outro={:.1f}s, ratio={:.3f})",
                 spotify_progress_ms, seek_ms, intro_end, outro_start, ratio);
    return seek_ms;
}
