// Exercise playback beyond the old 20-second chunk boundary. Each new section
// request costs longer than a chunk, while one continuous source can keep up.
#include "plugins/Video/desktop/VideoDesktop.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    const auto root = fs::temp_directory_path() / ("video-buffering-" + std::to_string(getpid()));
    fs::create_directories(root / "bin");
    fs::create_directories(root / "home");
    auto executable = [&](const char* name, const char* script) {
        const auto path = root / "bin" / name;
        std::ofstream(path) << script;
        chmod(path.c_str(), 0755);
    };
    executable("yt-dlp", R"SH(#!/bin/sh
if [ "$1" = "--version" ]; then echo fake; exit 0; fi
for arg in "$@"; do
  if [ "$arg" = "--dump-single-json" ]; then
    echo '{"url":"https://example.invalid/continuous","http_headers":{"User-Agent":"video-test"}}'
    exit 0
  fi
done
out=""; prev=""
for arg in "$@"; do
  [ "$prev" = "-o" ] && out="$arg"
  case "$arg" in '*20-40') sleep 25 ;; esac
  prev="$arg"
done
: > "$out"
)SH");
    executable("ffmpeg", R"SH(#!/bin/sh
if [ "$1" = "-version" ]; then echo fake; exit 0; fi
input=""; prev=""
for arg in "$@"; do
  [ "$prev" = "-i" ] && input="$arg"
  prev="$arg"; last="$arg"
done
if [ "$last" = "pipe:1" ]; then
  case "$input" in
    https:*) python3 -c 'import sys
for i in range(900): sys.stdout.buffer.write(i.to_bytes(3, "little") * 16384)' ;;
    *) dd if=/dev/zero bs=49152 count=120 2>/dev/null ;;
  esac
else
  dd if=/dev/zero of="$last" bs=49152 count=600 2>/dev/null
fi
)SH");
    const char* old_path = std::getenv("PATH");
    const auto path = (root / "bin").string() + ":" + (old_path ? old_path : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);
    std::mutex mutex;
    bool started = false;
    bool interrupted = false;
    Plugins::DesktopPlugin::set_message_sink([&](const auto&, const auto& message) {
        std::lock_guard lock(mutex);
        if (message == "status:playing") started = true;
        else if (started && message == "status:downloading") interrupted = true;
    });
    bool passed;
    {
        VideoDesktop video;
        video.post_init();
        video.on_websocket_message("stream:start");
        video.on_websocket_message("url:https://example.invalid/video");
        std::this_thread::sleep_for(22s);
        {
            std::lock_guard lock(mutex);
            passed = started && !interrupted;
        }
        const auto packet = video.compute_next_packet("video");
        if (packet) {
            const auto bytes = (*packet)->toData();
            const unsigned frame = bytes[0] | (unsigned(bytes[1]) << 8) | (unsigned(bytes[2]) << 16);
            passed = passed && frame >= 600;
        } else passed = false;
        video.on_websocket_message("stream:stop");
    }
    Plugins::DesktopPlugin::set_message_sink({});
    fs::remove_all(root);
    if (!passed) std::cerr << "FAIL: playback returned to loading at the chunk boundary\n";
    return passed ? 0 : 1;
}
