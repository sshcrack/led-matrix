#include "plugins/Video/desktop/VideoDesktop.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

int main() {
#ifdef _WIN32
    return 0;
#else
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("video-startup-" + std::to_string(getpid()));
    fs::create_directories(root / "bin");
    fs::create_directories(root / "home");
    auto executable = [&](const char* name, const char* script) {
        const auto path = root / "bin" / name;
        std::ofstream(path) << script;
        chmod(path.c_str(), 0755);
    };
    // Model a connection that can fetch a short chunk before fast-start ends,
    // but takes longer than fast-start to fetch five minutes of video.
    executable("yt-dlp", R"SH(#!/bin/sh
if [ "$1" = "--version" ]; then echo fake; exit 0; fi
out=""
prev=""
long=0
for arg in "$@"; do
  [ "$prev" = "-o" ] && out="$arg"
  [ "$arg" = "*0-300" ] && long=1
  prev="$arg"
done
[ "$long" = 1 ] && sleep 8
: > "$out"
)SH");
    executable("ffmpeg", R"SH(#!/bin/sh
if [ "$1" = "-version" ]; then echo fake; exit 0; fi
for arg in "$@"; do last="$arg"; done
if [ "$last" = "pipe:1" ]; then
  dd if=/dev/zero bs=49152 count=120 2>/dev/null
else
  dd if=/dev/zero of="$last" bs=49152 count=600 2>/dev/null
fi
)SH");
    const char* old_path = std::getenv("PATH");
    const auto path = (root / "bin").string() + ":" + (old_path ? old_path : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (root / "home").c_str(), 1);
    std::mutex mutex;
    std::string status;
    Plugins::DesktopPlugin::set_message_sink([&](const auto&, const auto& message) {
        std::lock_guard lock(mutex);
        status = message;
    });
    bool passed = false;
    {
        VideoDesktop video;
        video.post_init();
        video.on_websocket_message("stream:start");
        video.on_websocket_message("url:https://example.invalid/driving-monke");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        {
            std::lock_guard lock(mutex);
            passed = status == "status:playing";
            if (!passed) std::cerr << "Video returned to loading after fast-start: " << status << '\n';
        }
        passed = passed && video.compute_next_packet("video").has_value();
        video.on_websocket_message("stream:stop");
    }
    Plugins::DesktopPlugin::set_message_sink({});
    fs::remove_all(root);
    return passed ? 0 : 1;
#endif
}
