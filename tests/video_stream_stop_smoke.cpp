#include <shared/desktop/VideoStreamEngine.h>

#include <atomic>
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
void write_executable(const fs::path& path, const std::string& contents)
{
    std::ofstream out(path);
    out << contents;
    out.close();
    chmod(path.c_str(), 0755);
}
}
#endif

int main()
{
#ifdef _WIN32
    // The cancellation regression was in the POSIX process/thread path. Keep
    // this smoke test Linux-focused; Windows has separate process handling.
    return 0;
#else
    const auto root = fs::temp_directory_path() /
        ("led-matrix-video-stop-" + std::to_string(static_cast<long long>(::getpid())));
    const auto bin = root / "bin";
    const auto home = root / "home";
    fs::create_directories(bin);
    fs::create_directories(home);

    write_executable(bin / "yt-dlp", R"SH(#!/bin/sh
if [ "$1" = "--version" ]; then echo fake-yt-dlp; exit 0; fi
out=""
prev=""
for arg in "$@"; do
  if [ "$prev" = "-o" ]; then out="$arg"; break; fi
  prev="$arg"
done
case "$out" in
  *fast_chunk.mp4) : > "$out"; exit 0 ;;
  *) sleep 10; : > "$out"; exit 0 ;;
esac
)SH");

    write_executable(bin / "ffmpeg", R"SH(#!/bin/sh
if [ "$1" = "-version" ]; then echo fake-ffmpeg; exit 0; fi
case "$*" in
  *pipe:1*)
    dd if=/dev/zero bs=49152 count=2 2>/dev/null
    sleep 10
    exit 0
    ;;
  *)
    # Conversion output is the final argument. It should not normally be
    # reached in this test because stop() cancels the full download first.
    eval "last=\${$#}"
    dd if=/dev/zero of="$last" bs=49152 count=2 2>/dev/null
    exit 0
    ;;
esac
)SH");

    const char* old_path = std::getenv("PATH");
    const std::string path = bin.string() + ":" + (old_path ? old_path : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", home.c_str(), 1);

    Shared::VideoStreamEngine engine(root / "cache", 128, 128, 30.0);
    engine.set_chunk_duration_sec(20);
    engine.set_fast_chunk_duration_sec(2);
    std::atomic<bool> first_frame{false};

    if (const auto tools = engine.check_tools(); !tools.empty()) {
        std::cerr << tools << '\n';
        return 2;
    }

    engine.start("https://example.invalid/video", "stop-regression", 0);
    // start() calls stop(), which deliberately clears callbacks. SpotifyMV
    // reinstalls them immediately; mirror the production sequence.
    engine.on_first_frame_ready = [&] { first_frame = true; };

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!first_frame.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!first_frame.load()) {
        std::cerr << "fake fast-start never produced its first frame\n";
        engine.stop();
        return 3;
    }

    // This used to call std::terminate(): the fast-start path observed running
    // becoming false and broke out while its full-chunk std::thread was still
    // joinable. Returning from stop() without aborting is the regression signal.
    engine.stop();
    fs::remove_all(root);
    std::cout << "video stream stop joins in-flight full-chunk worker\n";
    return 0;
#endif
}
