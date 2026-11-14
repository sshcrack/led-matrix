// Exercise the real desktop plugin/decoder through its wire commands. External
// tools provide identifiable frames and a decoder that stalls before frame one.
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include "plugins/Video/desktop/VideoDesktop.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

int main() {
    const auto root = fs::temp_directory_path() / ("video-switch-" + std::to_string(getpid()));
    fs::create_directories(root / "bin");
    fs::create_directories(root / "home");
    auto executable = [&](const char* name, const char* contents) {
        const auto file = root / "bin" / name;
        std::ofstream(file) << contents;
        chmod(file.c_str(), 0755);
    };
    executable("yt-dlp", R"SH(#!/bin/sh
if [ "$1" = "--version" ]; then echo fake; exit 0; fi
out=""; prev=""; url=""
for arg in "$@"; do
  [ "$prev" = "-o" ] && out="$arg"
  prev="$arg"; url="$arg"
done
for arg in "$@"; do
  if [ "$arg" = "--dump-single-json" ]; then
    printf '{"url":"%s","http_headers":{}}\n' "$url"
    exit 0
  fi
done
printf '%s' "$url" > "$out"
)SH");
    executable("ffmpeg", R"SH(#!/bin/sh
if [ "$1" = "-version" ]; then echo fake; exit 0; fi
in=""; prev=""; last=""
for arg in "$@"; do
  [ "$prev" = "-i" ] && in="$arg"
  prev="$arg"; last="$arg"
done
case "$in" in https:*) url="$in" ;; *) url="$(cat "$in")" ;; esac
color=42
case "$url" in *other*) color=180 ;; esac
case "$url" in
  *slow*)
    : > "$VIDEO_TEST_ROOT/decoder-started"
    sleep 8
    ;;
esac
if [ "$last" = "pipe:1" ]; then
  exec cat "$VIDEO_TEST_ROOT/$color.rgb"
fi
cp "$VIDEO_TEST_ROOT/$color.rgb" "$last"
)SH");
    for (int color : {42, 180}) {
        std::vector<char> frames(128 * 128 * 3 * 150, static_cast<char>(color));
        std::ofstream(root / (std::to_string(color) + ".rgb"), std::ios::binary)
            .write(frames.data(), static_cast<std::streamsize>(frames.size()));
    }
    const char* previous = std::getenv("PATH");
    const auto path = (root / "bin").string() + ":" + (previous ? previous : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);
    setenv("VIDEO_TEST_ROOT", root.c_str(), 1);
    Plugins::DesktopPlugin::set_message_sink([](const auto&, const auto&) {});
    int result = 0;
    {
        VideoDesktop plugin;
        plugin.post_init();
        plugin.on_websocket_message("stream:start");
        plugin.on_websocket_message("url:https://example.invalid/slow");
        auto deadline = std::chrono::steady_clock::now() + 2s;
        while (!fs::exists(root / "decoder-started") && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(10ms);
        if (!fs::exists(root / "decoder-started")) {
            std::cerr << "FAIL: stalled decoder did not start\n";
            result = 1;
        }
        const auto start = std::chrono::steady_clock::now();
        plugin.on_websocket_message("stream:stop");
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > 1500ms) {
            std::cerr << "FAIL: switching away from a decoder with no frames blocked for "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms\n";
            result = 1;
        }
        plugin.on_websocket_message("stream:start");
        plugin.on_websocket_message("url:https://example.invalid/fast");
        bool received = false;
        deadline = std::chrono::steady_clock::now() + 2s;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto packet = plugin.compute_next_packet("video");
            if (packet && (*packet)->toData().front() == 42) { received = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        if (!received) { std::cerr << "FAIL: replacement video never produced its frame\n"; result = 1; }
        // A direct URL change while playing must replace the decoded pixels,
        // including when returning to a URL already present in the cache.
        for (const auto& [url, color] : std::vector<std::pair<std::string, int>>{
                 {"other", 180}, {"fast", 42}, {"other", 180}}) {
            plugin.on_websocket_message("url:https://example.invalid/" + url);
            bool matching = false;
            deadline = std::chrono::steady_clock::now() + 2s;
            while (std::chrono::steady_clock::now() < deadline) {
                const auto packet = plugin.compute_next_packet("video");
                if (packet && (*packet)->toData().front() == color) { matching = true; break; }
                std::this_thread::sleep_for(10ms);
            }
            if (!matching) { std::cerr << "FAIL: wrong pixels after URL change to " << url << '\n'; result = 1; }
        }
        // The matrix announces its size on the WebSocket thread while the UDP
        // loop builds packets and the UI thread polls the plugin every frame.
        {
            std::atomic<bool> running{true};
            std::atomic<int> packets{0};
            std::thread udp([&] {
                while (running) {
                    packets += static_cast<int>(plugin.compute_next_packets("video").size());
                    std::this_thread::sleep_for(1ms);
                }
            });
            std::thread ui([&] {
                while (running) {
                    plugin.pre_new_frame();
                    std::this_thread::sleep_for(1ms);
                }
            });
            for (int i = 0; i < 200; ++i) {
                plugin.on_websocket_message("size:128x128");
                std::this_thread::sleep_for(1ms);
            }
            running = false;
            udp.join();
            ui.join();
            if (packets.load() == 0) {
                std::cerr << "FAIL: no packets while the size was announced\n";
                result = 1;
            }
        }
        plugin.on_websocket_message("stream:stop");
        if (plugin.compute_next_packet("video")) {
            std::cerr << "FAIL: stopped video continues emitting frames\n";
            result = 1;
        }
    }
    Plugins::DesktopPlugin::set_message_sink({});
    fs::remove_all(root);
    return result;
}
