// Thread- and process-lifetime regressions for the shared desktop layer that
// plugins sit on (2026-11-14 audit, docs/memory-safety-report.md). Each case
// reproduces one finding and is most useful under the desktop-linux-tsan and
// desktop-linux-asan presets, which report the race / use-after-free directly;
// the plain build still checks the observable result.
//
//  1. A plugin thread sending a WebSocket message while the UI thread clears
//     and destroys the client at shutdown (WebsocketClient::sendPluginMessage).
//  2. VideoStreamEngine::get_current_url() (UI thread) against start() (search
//     thread).
//  3. File descriptors of the desktop (sockets, the ffmpeg pipe of another
//     thread) leaking into yt-dlp / ffmpeg children.

#include <shared/desktop/VideoStreamEngine.h>
#include <shared/desktop/WebsocketClient.h>
#include <shared/desktop/plugin/main.h>
#include <shared/desktop/utils.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
    else {
        std::cout << "ok: " << what << '\n';
    }
}

class ProbePlugin final : public Plugins::DesktopPlugin {
public:
    void render() override {}
    void initialize_imgui(ImGuiContext*, ImGuiMemAllocFunc*, ImGuiMemFreeFunc*, void**) override {}
    std::string get_plugin_name() const override { return "ThreadingProbe"; }
};

void websocket_send_survives_client_shutdown()
{
    std::cout << "-- plugin threads sending while the client is cleared and destroyed\n";

    // A client that is destroyed while registered must unregister itself.
    {
        auto client = WebsocketClient::create();
        WebsocketClient::setInstance(client.get());
        client.reset();
        expect(WebsocketClient::instance() == nullptr, "destroying the active client clears the instance");
    }

    // Plugin worker threads (search, engine status callbacks) send at any time;
    // main.cpp clears the instance and destroys the client on the UI thread.
    ProbePlugin plugin;
    std::atomic<bool> stop{false};
    std::atomic<long> sends{0};
    std::vector<std::thread> senders;
    for (int i = 0; i < 3; ++i) {
        senders.emplace_back([&] {
            while (!stop.load()) {
                plugin.send_websocket_message("status:playing");
                ++sends;
            }
        });
    }

    constexpr int kRounds = 1500;
    for (int i = 0; i < kRounds; ++i) {
        auto client = WebsocketClient::create();
        WebsocketClient::setInstance(client.get());
        std::this_thread::yield();
        WebsocketClient::setInstance(nullptr);
        client.reset();
    }
    stop = true;
    for (auto& sender : senders) sender.join();
    expect(sends.load() > 0, "plugin threads kept sending throughout " + std::to_string(kRounds) + " client lifetimes");
}

void engine_url_is_safe_to_read_while_starting()
{
    std::cout << "-- VideoStreamEngine::get_current_url() against start()\n";
    const auto root = fs::temp_directory_path() / ("led-matrix-engine-url-" + std::to_string(static_cast<long long>(::getpid())));
    fs::create_directories(root);

    Shared::VideoStreamEngine engine(root / "cache", 128, 128);
    std::atomic<bool> stop{false};
    std::atomic<long> reads{0};
    std::thread ui([&] {
        while (!stop.load()) {
            const auto url = engine.get_current_url();
            if (!url.empty() && !url.starts_with("https://example.invalid/"))
                std::cerr << "unexpected url '" << url << "'\n";
            ++reads;
        }
    });

    for (int i = 0; i < 60; ++i) {
        // Long enough to reallocate the string, so a torn read is visible.
        engine.start("https://example.invalid/" + std::string(40 + (i % 7) * 30, 'a') + std::to_string(i), "url-" + std::to_string(i));
        std::this_thread::sleep_for(2ms);
    }
    engine.stop();
    stop = true;
    ui.join();
    expect(reads.load() > 0, "the UI thread read the url throughout 60 starts");
    fs::remove_all(root);
}

void engine_callbacks_are_safe_to_replace_while_firing()
{
    std::cout << "-- VideoStreamEngine callbacks replaced while the worker reports status\n";
    const auto root = fs::temp_directory_path() / ("led-matrix-engine-cb-" + std::to_string(static_cast<long long>(::getpid())));
    fs::create_directories(root);

    Shared::VideoStreamEngine engine(root / "cache", 128, 128);
    std::atomic<long> status_calls{0};
    for (int i = 0; i < 40; ++i) {
        engine.set_on_status_change([&](const std::string&) { ++status_calls; });
        engine.start("https://example.invalid/cb" + std::to_string(i), "cb-" + std::to_string(i));
        // The worker reports "downloading" and then "error"/"idle" while these
        // run on this thread, as SpotifyMV does when a track is superseded.
        for (int j = 0; j < 20; ++j) {
            engine.clear_callbacks();
            engine.set_on_status_change([&](const std::string&) { ++status_calls; });
            engine.set_on_first_frame_ready([] {});
            std::this_thread::sleep_for(100us);
        }
        engine.clear_callbacks();
        engine.stop();
    }
    expect(true, "callbacks can be cleared and replaced while workers report (" + std::to_string(status_calls.load()) + " reports)");
    fs::remove_all(root);
}

#ifndef _WIN32
void write_executable(const fs::path& path, const std::string& body)
{
    std::ofstream(path) << body;
    chmod(path.c_str(), 0755);
}

void children_do_not_inherit_descriptors()
{
    std::cout << "-- yt-dlp / ffmpeg children do not inherit the desktop's descriptors\n";
    const auto root = fs::temp_directory_path() / ("led-matrix-child-fds-" + std::to_string(static_cast<long long>(::getpid())));
    fs::remove_all(root);
    fs::create_directories(root / "bin");

    // Stands in for the WebSocket / UDP sockets (ixwebsocket does not set
    // CLOEXEC) and for another thread's pipe: opened without close-on-exec.
    const int marker = ::open("/dev/null", O_RDONLY);
    expect(marker > 2 && !(fcntl(marker, F_GETFD) & FD_CLOEXEC), "marker descriptor is inheritable");

    {
        const auto result = run_command_capture("test ! -e /proc/$$/fd/" + std::to_string(marker));
        expect(result.exit_code == 0, "run_command_capture child does not see an inheritable descriptor");
    }

    // The engine's decoder pipe (play_decoded_pipe) runs the fake ffmpeg below;
    // every fake tool records whether it was handed the marker descriptor.
    const std::string probe =
        "if [ -e /proc/$$/fd/" + std::to_string(marker) + " ]; then echo leaked >> '" + (root / "leaks.txt").string() + "'; fi\n";
    write_executable(root / "bin" / "yt-dlp", "#!/bin/sh\n" + probe + R"SH(
case "$*" in *--version*) echo fake-yt-dlp; exit 0 ;; esac
out=""; prev=""
for a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done
[ -n "$out" ] && : > "$out"
exit 0
)SH");
    write_executable(root / "bin" / "ffmpeg", "#!/bin/sh\n" + probe + R"SH(
case "$*" in *-version*) echo fake-ffmpeg; exit 0 ;; esac
last=""; for a in "$@"; do last="$a"; done
if [ "$last" = "pipe:1" ]; then exec head -c 147456 /dev/zero; fi
head -c 147456 /dev/zero > "$last"
)SH");

    const char* old_path = std::getenv("PATH");
    setenv("PATH", ((root / "bin").string() + ":" + (old_path ? old_path : "")).c_str(), 1);

    Shared::VideoStreamEngine engine(root / "cache", 128, 128, 120.0);
    std::atomic<bool> first_frame{false};
    engine.set_on_first_frame_ready([&] { first_frame = true; });
    engine.start("https://example.invalid/video", "child-fds");
    const auto deadline = std::chrono::steady_clock::now() + 15s;
    while (!first_frame.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(20ms);
    expect(first_frame.load(), "the fake decoder produced a frame");
    engine.stop();
    expect(!fs::exists(root / "leaks.txt"), "yt-dlp and ffmpeg started by the engine do not inherit the descriptor");

    if (old_path)
        setenv("PATH", old_path, 1);
    ::close(marker);
    fs::remove_all(root);
}
#endif

}  // namespace

int main()
{
#ifndef _WIN32
    const auto home = fs::temp_directory_path() / ("led-matrix-threading-home-" + std::to_string(static_cast<long long>(::getpid())));
    fs::create_directories(home);
    setenv("HOME", home.c_str(), 1);
#endif

    websocket_send_survives_client_shutdown();
    engine_url_is_safe_to_read_while_starting();
    engine_callbacks_are_safe_to_replace_while_firing();
#ifndef _WIN32
    children_do_not_inherit_descriptors();
    fs::remove_all(home);
#endif

    if (failures) {
        std::cerr << failures << " desktop threading check(s) failed\n";
        return 1;
    }
    std::cout << "desktop threading OK\n";
    return 0;
}
