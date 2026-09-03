#include "plugins/SpotifyMV/desktop/SpotifyMVDesktop.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

#ifndef _WIN32
namespace {
void write_executable(const fs::path& path, const std::string& body)
{
    std::ofstream out(path);
    out << body;
    out.close();
    chmod(path.c_str(), 0755);
}
}
#endif

int main()
{
#ifdef _WIN32
    return 0;
#else
    const auto root = fs::temp_directory_path() /
        ("led-matrix-spotify-stop-" + std::to_string(static_cast<long long>(::getpid())));
    const auto bin = root / "bin";
    const auto home = root / "home";
    fs::create_directories(bin);
    fs::create_directories(home);

    write_executable(bin / "yt-dlp", R"SH(#!/bin/sh
if [ "$1" = "--version" ]; then echo fake-yt-dlp; exit 0; fi
case "$*" in
  *ytsearch1:*) sleep 3; echo 'https://www.youtube.com/watch?v=fake'; exit 0 ;;
  *) echo 180; exit 0 ;;
esac
)SH");
    write_executable(bin / "ffmpeg", R"SH(#!/bin/sh
if [ "$1" = "-version" ]; then echo fake-ffmpeg; exit 0; fi
exit 0
)SH");

    const char* old_path = std::getenv("PATH");
    const std::string path = bin.string() + ":" + (old_path ? old_path : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", home.c_str(), 1);

    SpotifyMVDesktop plugin;
    plugin.post_init();
    plugin.on_websocket_message("track:track-a:Song\nArtist\nofficial music video\ntrue\n0\n180000");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto started = std::chrono::steady_clock::now();
    plugin.on_websocket_message("stop");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);

    fs::remove_all(root);
    if (elapsed > std::chrono::milliseconds(500)) {
        std::cerr << "SpotifyMV stop blocked the WebSocket callback for "
                  << elapsed.count() << " ms\n";
        return 1;
    }
    std::cout << "SpotifyMV stop returned in " << elapsed.count() << " ms\n";
    return 0;
#endif
}
