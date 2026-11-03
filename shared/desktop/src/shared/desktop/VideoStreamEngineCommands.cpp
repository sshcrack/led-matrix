#include "shared/desktop/VideoStreamEngine.h"
#include "VideoStreamEngineInternal.h"
#include "shared/desktop/utils.h"
#include <algorithm>
#include <cstdio>
#include <fmt/format.h>
#include <thread>
#include <spdlog/spdlog.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace Shared {

namespace detail {

std::string command_error_summary(const std::string& output) {
    std::string error_line;
    std::string fallback_line;
    std::string line;

    auto consider = [&](std::string candidate) {
        candidate.erase(std::remove_if(candidate.begin(), candidate.end(), [](unsigned char c) {
            return c < 0x20 && c != '\t';
        }), candidate.end());
        const auto first = candidate.find_first_not_of(" \t");
        if (first == std::string::npos)
            return;
        candidate.erase(0, first);
        const auto last = candidate.find_last_not_of(" \t");
        if (last != std::string::npos)
            candidate.erase(last + 1);
        if (candidate.empty())
            return;
        fallback_line = candidate;
        if (candidate.find("ERROR:") != std::string::npos)
            error_line = candidate;
    };

    for (char c : output) {
        if (c == '\n' || c == '\r') {
            consider(std::move(line));
            line.clear();
        } else {
            line.push_back(c);
        }
    }
    consider(std::move(line));

    std::string result = !error_line.empty() ? error_line : fallback_line;
    constexpr std::size_t MaxSummary = 700;
    if (result.size() > MaxSummary)
        result = result.substr(0, MaxSummary - 3) + "...";
    return result;
}

} // namespace detail

// ---- Command construction helpers ----
std::string VideoStreamEngine::build_ytdlp_command(
    const std::filesystem::path& output_path, int start_sec, int end_sec) const
{
    return fmt::format(
        "{} -f \"{}\" --download-sections \"*{}-{}\" "
        "--force-overwrites -o \"{}\" \"{}\"",
        get_ytdlp_network_command(), get_ytdlp_video_format_selector(),
        start_sec, end_sec, output_path.string(), current_url_);
}

std::string VideoStreamEngine::build_ffmpeg_command(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path) const
{
    return fmt::format(
        "ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1,fps={}\" "
        "-f rawvideo -pix_fmt rgb24 \"{}\"",
        input_path.string(), width_, height_, fps_, output_path.string());
}

std::string VideoStreamEngine::build_ffmpeg_pipe_command(
    const std::filesystem::path& input_path) const
{
#ifdef _WIN32
    return fmt::format(
        "ffmpeg -y -i \"{}\" -vf \"scale={}:{},setsar=1:1,fps={}\" "
        "-f rawvideo -pix_fmt rgb24 pipe:1 2>NUL",
        input_path.string(), width_, height_, fps_);
#else
    return fmt::format(
        "ffmpeg -y -i '{}' -vf 'scale={}:{},setsar=1:1,fps={}' "
        "-f rawvideo -pix_fmt rgb24 pipe:1 2>/dev/null",
        input_path.string(), width_, height_, fps_);
#endif
}

} // namespace Shared
