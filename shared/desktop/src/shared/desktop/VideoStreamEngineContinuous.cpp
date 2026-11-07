#include "shared/desktop/VideoStreamEngine.h"
#include "shared/desktop/utils.h"
#include "VideoStreamEngineInternal.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>

namespace Shared {
namespace {
// FFmpeg is launched directly on Windows and through sh on POSIX.
std::string argument(const std::string& value) {
#ifdef _WIN32
    std::string result = "\"";
    size_t backslashes = 0;
    for (char c : value) {
        if (c == '\\') { ++backslashes; continue; }
        result.append(c == '"' ? 2 * backslashes + 1 : backslashes, '\\');
        result += c;
        backslashes = 0;
    }
    result.append(2 * backslashes, '\\');
    return result + '"';
#else
    std::string result = "'";
    for (char c : value) result += c == '\'' ? "'\\''" : std::string(1, c);
    return result + '\'';
#endif
}
}

void VideoStreamEngine::play_continuous_stream(long seek_ms) {
    state_ = State::Downloading;
    notify_status("downloading");
    spdlog::info("Video: resolving continuous source");
    // Resolve once, including the headers required by the selected CDN source.
    // Keep signed media URLs out of logs. yt-dlp's JSON can include a large
    // formats list, hence the larger bounded capture than normal diagnostics.
    const auto source = run_command_capture(fmt::format(
        "{} --no-playlist --no-warnings -f \"{}\" --dump-single-json {}",
        get_ytdlp_network_command(), get_ytdlp_video_format_selector(), argument(current_url_)),
        &running_, 4 * 1024 * 1024);
    if (!running_.load()) return;
    if (source.exit_code != 0) {
        set_last_error("Could not resolve video source: " + detail::command_error_summary(source.output));
        state_ = State::Error;
        notify_status("error");
        return;
    }

    nlohmann::json metadata;
    std::istringstream lines(source.output);
    for (std::string line; std::getline(lines, line);) {
        auto candidate = nlohmann::json::parse(line, nullptr, false);
        if (candidate.is_object() && candidate.contains("url")) {
            metadata = std::move(candidate);
            break;
        }
    }
    if (!metadata.is_object() || !metadata["url"].is_string())
        throw std::runtime_error("yt-dlp did not return a continuous video source");
    const auto url = metadata["url"].get<std::string>();
    // The selected video-only format must be an HTTP source, not an arbitrary
    // FFmpeg protocol supplied by extractor metadata.
    if (!url.starts_with("https://") && !url.starts_with("http://"))
        throw std::runtime_error("Video source is not HTTP(S)");

    std::string headers;
    if (metadata.contains("http_headers") && metadata["http_headers"].is_object()) {
        for (const auto& [name, value] : metadata["http_headers"].items()) {
            if (!value.is_string()) continue;
            const auto text = value.get<std::string>();
            if (name.find_first_of("\r\n:") != std::string::npos ||
                text.find_first_of("\r\n") != std::string::npos) continue;
            headers += name + ": " + text + "\r\n";
        }
    }
    // FFmpeg reads and decodes incrementally. Pipe backpressure bounds memory
    // to a few frames, even for multi-hour ambience videos. Loop in the same
    // decoder, with no extraction/download handoff at an artificial boundary.
    auto command = fmt::format("ffmpeg -nostdin -loglevel error -stream_loop -1 -rw_timeout 30000000 {}{}-i {} "
                               "-an -vf {} -f rawvideo -pix_fmt rgb24 pipe:1",
        headers.empty() ? "" : "-headers " + argument(headers) + " ",
        seek_ms > 0 ? fmt::format("-ss {:.3f} ", seek_ms / 1000.0) : "",
        argument(url), argument(fmt::format("scale={}:{},setsar=1:1,fps={}", width_, height_, fps_)));
#ifndef _WIN32
    command += " 2>/dev/null";
#endif
    spdlog::info("Video: starting continuous decoder ({}x{} @ {}fps)", width_, height_, fps_);
    play_decoded_pipe(command);
    if (running_.load()) {
        set_last_error("Continuous video decoder stopped unexpectedly");
        state_ = State::Error;
        notify_status("error");
    }
}

} // namespace Shared
