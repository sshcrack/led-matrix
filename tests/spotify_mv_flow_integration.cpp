// End-to-end flows of the SpotifyMV desktop producer: the real plugin, the real
// VideoStreamEngine (fast start, chunking, cache, crossfade) and its process
// handling, driven by the same WebSocket messages the Pi sends. Only yt-dlp and
// ffmpeg are fakes: shell scripts on PATH that "find" one video per song and
// decode it into frames whose first byte identifies the track and whose second
// byte is the frame index, so the test can see exactly what the matrix would
// show and whether it is moving.
//
// Knobs the fake tools read from $FAKE_MV_ROOT:
//   search_delay_<song>   seconds the YouTube search takes
//   no_results_<song>     the search finds nothing
//   frames/<track>.rgb    the decoded frames of that track's video
//   ytdlp.log             every yt-dlp invocation (used for cache checks)

#include "plugins/SpotifyMV/desktop/SpotifyMVDesktop.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

#ifndef _WIN32
namespace {

int failures = 0;
fs::path root;

void expect(bool ok, const std::string& what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    } else {
        std::cout << "ok: " << what << '\n';
    }
}

void write_executable(const fs::path& path, const std::string& body)
{
    std::ofstream(path) << body;
    chmod(path.c_str(), 0755);
}

void touch(const fs::path& path, const std::string& content = "")
{
    std::ofstream(path) << content;
}

constexpr int kFrameBytes = 128 * 128 * 3;
constexpr int kFramesPerClip = 30; // 1 s at 30 fps

// Distinct, well separated colors so crossfade blends are recognizable.
const std::vector<std::pair<std::string, int>> kTracks = {
    {"A", 40}, {"B", 200}, {"C", 90}, {"D", 120}, {"E", 150}, {"F", 240}, {"G", 20},
};

int color_of(const std::string& track)
{
    for (const auto& [id, color] : kTracks)
        if (id == track) return color;
    return -1;
}

void write_fake_tools()
{
    const auto bin = root / "bin";
    fs::create_directories(bin);
    fs::create_directories(root / "frames");

    // yt-dlp: version probe, search (song = first word of the query), duration,
    // and section downloads (creates the -o file; ffmpeg ignores its content).
    write_executable(bin / "yt-dlp", R"SH(#!/bin/sh
R="$FAKE_MV_ROOT"
echo "$*" >> "$R/ytdlp.log"
case "$*" in
  *--version*) echo fake-yt-dlp; exit 0 ;;
  *ytsearch1:*)
    q="${*#*ytsearch1:}"; song="${q%% *}"
    if [ -f "$R/search_delay_$song" ]; then sleep "$(cat "$R/search_delay_$song")"; fi
    if [ -f "$R/no_results_$song" ]; then exit 0; fi
    echo "https://www.youtube.com/watch?v=vid$song"; exit 0 ;;
  *"--print duration"*) echo 180; exit 0 ;;
esac
out=""; prev=""
for a in "$@"; do [ "$prev" = "-o" ] && out="$a"; prev="$a"; done
[ -n "$out" ] && : > "$out"
exit 0
)SH");

    // ffmpeg: the cache directory of the input is the track id. Pipe mode
    // (fast start) streams one clip; file mode writes a 3-clip chunk. `exec`
    // keeps one process per invocation so stop() can kill the producer.
    write_executable(bin / "ffmpeg", R"SH(#!/bin/sh
R="$FAKE_MV_ROOT"
in=""; prev=""; last=""
for a in "$@"; do [ "$prev" = "-i" ] && in="$a"; prev="$a"; last="$a"; done
case "$*" in *-version*) echo fake-ffmpeg; exit 0 ;; esac
track="$(basename "$(dirname "$in")")"
f="$R/frames/$track.rgb"
if [ "$last" = "pipe:1" ]; then exec cat "$f"; fi
cat "$f" "$f" "$f" > "$last"
)SH");

    for (const auto& [id, color] : kTracks) {
        std::vector<char> frames(static_cast<size_t>(kFrameBytes) * kFramesPerClip);
        for (int i = 0; i < kFramesPerClip; ++i) {
            char* frame = frames.data() + static_cast<size_t>(i) * kFrameBytes;
            std::fill(frame, frame + kFrameBytes, static_cast<char>(color));
            frame[1] = static_cast<char>(i);
        }
        std::ofstream(root / "frames" / (id + ".rgb"), std::ios::binary)
            .write(frames.data(), static_cast<std::streamsize>(frames.size()));
    }
}

int count_in_ytdlp_log(const std::string& needle)
{
    std::ifstream in(root / "ytdlp.log");
    std::string line;
    int n = 0;
    while (std::getline(in, line))
        if (line.find(needle) != std::string::npos) ++n;
    return n;
}

/// The plugin plus what the Pi would observe: its messages, and the frames the
/// UDP loop would send while `spotifymv` is the active scene.
class Harness {
public:
    explicit Harness(int crossfade_ms = 0)
    {
        Plugins::DesktopPlugin::set_message_sink([this](const std::string&, const std::string& msg) {
            std::lock_guard lock(mutex_);
            messages_.push_back(msg);
        });
        plugin_.load_config(nlohmann::json{{"crossfade_duration_ms", crossfade_ms}});
        plugin_.post_init();
        // The desktop's UI thread: pre_new_frame() runs every frame, concurrently
        // with the WebSocket thread (these tests' main thread) and the UDP loop.
        ui_ = std::thread([this] {
            while (pumping_) {
                plugin_.pre_new_frame();
                std::this_thread::sleep_for(1ms);
            }
        });
        pump_ = std::thread([this] {
            while (pumping_) {
                auto packet = plugin_.compute_next_packet("spotifymv");
                Frame frame;
                if (packet && *packet) {
                    const auto data = (*packet)->toData();
                    if (data.size() >= 2) frame = {data[0], data[1]};
                }
                {
                    std::lock_guard lock(mutex_);
                    frames_.push_back(frame);
                }
                std::this_thread::sleep_for(16ms);
            }
        });
    }

    ~Harness()
    {
        pumping_ = false;
        pump_.join();
        ui_.join();
        Plugins::DesktopPlugin::set_message_sink(nullptr);
    }

    struct Frame {
        int color = -1;
        int index = -1;
    };

    void play(const std::string& track) { plugin_.on_websocket_message(track_message(track)); }
    void send(const std::string& msg) { plugin_.on_websocket_message(msg); }

    static std::string track_message(const std::string& track)
    {
        // Song == track id so the fake search can be steered per track.
        // Duration 0 skips the SponsorBlock lookup (no network in tests).
        return "track:" + track + ":" + track + "\nArtist\nofficial music video\ntrue\n0\n0";
    }

    size_t mark() const
    {
        std::lock_guard lock(mutex_);
        return messages_.size();
    }
    size_t frame_mark() const
    {
        std::lock_guard lock(mutex_);
        return frames_.size();
    }

    bool has_message(const std::string& msg, size_t since = 0) const
    {
        std::lock_guard lock(mutex_);
        return std::find(messages_.begin() + static_cast<std::ptrdiff_t>(std::min(since, messages_.size())),
                         messages_.end(), msg) != messages_.end();
    }

    bool has_message_prefix(const std::string& prefix, size_t since = 0) const
    {
        std::lock_guard lock(mutex_);
        for (size_t i = since; i < messages_.size(); ++i)
            if (messages_[i].starts_with(prefix)) return true;
        return false;
    }

    bool wait_message(const std::string& msg, size_t since, std::chrono::milliseconds timeout = 6000ms) const
    {
        return wait_until([&] { return has_message(msg, since); }, timeout);
    }

    bool wait_color(int color, std::chrono::milliseconds timeout = 6000ms) const
    {
        return wait_until([&] { return last_frame().color == color; }, timeout);
    }

    Frame last_frame() const
    {
        std::lock_guard lock(mutex_);
        return frames_.empty() ? Frame{} : frames_.back();
    }

    std::vector<Frame> frames_since(size_t since) const
    {
        std::lock_guard lock(mutex_);
        return {frames_.begin() + static_cast<std::ptrdiff_t>(std::min(since, frames_.size())), frames_.end()};
    }

    std::string dump(size_t since = 0) const
    {
        std::lock_guard lock(mutex_);
        std::ostringstream out;
        for (size_t i = since; i < messages_.size(); ++i) out << "    " << messages_[i] << '\n';
        return out.str();
    }

    static bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(10ms);
        }
        return predicate();
    }

private:
    SpotifyMVDesktop plugin_;
    mutable std::mutex mutex_;
    std::vector<std::string> messages_;
    std::vector<Frame> frames_;
    std::atomic<bool> pumping_{true};
    std::thread pump_;
    std::thread ui_;
};

bool saw_color(const std::vector<Harness::Frame>& frames, int color)
{
    return std::any_of(frames.begin(), frames.end(), [&](const auto& f) { return f.color == color; });
}

int distinct_indices(const std::vector<Harness::Frame>& frames, int color)
{
    std::set<int> seen;
    for (const auto& f : frames)
        if (f.color == color) seen.insert(f.index);
    return static_cast<int>(seen.size());
}

void first_track_and_skip()
{
    std::cout << "-- first track, then skip with crossfade\n";
    Harness h(/*crossfade_ms=*/300);

    auto m = h.mark();
    h.play("A");
    expect(h.wait_message("track:ready:A", m), "A becomes ready");
    expect(h.has_message("track:preparing:A", m) && h.has_message("status:pending", m),
           "A is announced as preparing before it is ready");
    expect(h.wait_color(color_of("A")), "matrix shows A");
    auto fm = h.frame_mark();
    std::this_thread::sleep_for(700ms);
    expect(distinct_indices(h.frames_since(fm), color_of("A")) >= 5, "A is moving, not a frozen frame");

    m = h.mark();
    fm = h.frame_mark();
    h.play("B");
    expect(h.wait_message("track:ready:B", m), "B becomes ready");
    expect(h.wait_color(color_of("B")), "matrix shows B after the crossfade");
    const auto during = h.frames_since(fm);
    const bool blended = std::any_of(during.begin(), during.end(), [](const auto& f) {
        return f.color > color_of("A") && f.color < color_of("B");
    });
    expect(blended, "skip crossfades from A into B");

    fm = h.frame_mark();
    std::this_thread::sleep_for(700ms);
    const auto after = h.frames_since(fm);
    expect(!saw_color(after, color_of("A")), "A's engine no longer reaches the matrix");
    expect(distinct_indices(after, color_of("B")) >= 5, "B keeps moving after the swap");
}

void return_to_current_while_loading()
{
    std::cout << "-- skip, then go back before the new video loaded\n";
    Harness h;
    auto m = h.mark();
    h.play("A");
    expect(h.wait_message("track:ready:A", m), "A becomes ready");

    touch(root / "search_delay_C", "2");
    m = h.mark();
    h.play("C");
    expect(h.wait_message("track:preparing:C", m, 1000ms), "C starts preparing");
    std::this_thread::sleep_for(300ms);

    m = h.mark();
    const auto fm = h.frame_mark();
    h.play("A");
    expect(h.wait_message("status:playing", m, 1000ms), "returning to A reports playing at once");
    std::this_thread::sleep_for(2500ms);
    expect(!h.has_message_prefix("track:ready:C", m), "cancelled C never becomes ready");
    expect(!saw_color(h.frames_since(fm), color_of("C")), "cancelled C never reaches the matrix");
    expect(h.last_frame().color == color_of("A"), "A keeps playing");

    // Regression (#103): after skip-and-return A's engine was permanently
    // muted, so its later status (buffering, errors) never reached the Pi.
    // A's engine reports "playing" each time it starts a new chunk.
    const auto before = h.mark();
    expect(Harness::wait_until([&] { return h.has_message_prefix("status:", before); }, 8000ms),
           "A's engine status still reaches the Pi after the return");
    fs::remove(root / "search_delay_C");
}

void rapid_skips()
{
    std::cout << "-- rapid skips: only the last track may play\n";
    Harness h;
    auto m = h.mark();
    h.play("A");
    expect(h.wait_message("track:ready:A", m), "A becomes ready");

    touch(root / "search_delay_D", "1");
    touch(root / "search_delay_E", "1");
    m = h.mark();
    const auto fm = h.frame_mark();
    h.play("D");
    std::this_thread::sleep_for(100ms);
    h.play("E");
    std::this_thread::sleep_for(100ms);
    h.play("F");
    expect(h.wait_message("track:ready:F", m), "F becomes ready");
    expect(h.wait_color(color_of("F")), "matrix shows F");
    std::this_thread::sleep_for(1500ms);
    expect(!h.has_message("track:ready:D", m) && !h.has_message("track:ready:E", m),
           "superseded D and E never become ready");
    const auto frames = h.frames_since(fm);
    expect(!saw_color(frames, color_of("D")) && !saw_color(frames, color_of("E")),
           "superseded D and E never reach the matrix");
    expect(h.last_frame().color == color_of("F"), "F is still playing");
    fs::remove(root / "search_delay_D");
    fs::remove(root / "search_delay_E");
}

void search_failure_and_retry()
{
    std::cout << "-- no video found, current keeps playing, retry works\n";
    Harness h;
    auto m = h.mark();
    h.play("A");
    expect(h.wait_message("track:ready:A", m), "A becomes ready");

    touch(root / "no_results_G");
    m = h.mark();
    h.play("G");
    expect(h.wait_message("track:error:G", m), "missing video is reported as track:error");
    expect(h.has_message("status:error", m), "missing video is reported as status:error");
    std::this_thread::sleep_for(300ms);
    expect(h.last_frame().color == color_of("A"), "A keeps playing after G failed");

    // The Pi re-sends its last track message when the WebSocket reconnects.
    fs::remove(root / "no_results_G");
    m = h.mark();
    h.play("G");
    expect(h.wait_message("track:ready:G", m), "re-requesting a failed track retries it");
    expect(h.wait_color(color_of("G")), "matrix shows G after the retry");

    // A failed request must not block going back to the current track either.
    touch(root / "no_results_C");
    m = h.mark();
    h.play("C");
    expect(h.wait_message("track:error:C", m), "C fails");
    m = h.mark();
    h.play("G");
    std::this_thread::sleep_for(300ms);
    expect(!h.has_message("track:preparing:G", m), "current G is not re-downloaded after C failed");
    expect(h.last_frame().color == color_of("G"), "G still playing");
    fs::remove(root / "no_results_C");
}

void stop_and_cached_replay()
{
    std::cout << "-- stop, then replay from cache\n";
    Harness h;
    auto m = h.mark();
    h.play("B");
    expect(h.wait_message("track:ready:B", m), "B becomes ready");
    std::this_thread::sleep_for(500ms); // let the full chunk 0 land in the cache

    m = h.mark();
    h.send("stop");
    expect(h.has_message("track:idle", m) && h.has_message("status:idle", m), "stop reports idle");
    std::this_thread::sleep_for(100ms);
    expect(h.last_frame().color == -1, "nothing is sent to the matrix after stop");

    const int fast_downloads = count_in_ytdlp_log("/B/fast_chunk.mp4");
    m = h.mark();
    h.play("B");
    expect(h.wait_message("track:ready:B", m), "the same track plays again after stop");
    expect(h.wait_color(color_of("B")), "matrix shows B again");
    expect(count_in_ytdlp_log("/B/fast_chunk.mp4") == fast_downloads,
           "replay starts from the cached first chunk (no fast-start download)");
}

void producer_handover()
{
    std::cout << "-- producer standby/active\n";
    Harness h;
    auto m = h.mark();
    h.play("A");
    expect(h.wait_message("track:ready:A", m), "A becomes ready");

    h.send("producer:standby");
    std::this_thread::sleep_for(100ms);
    expect(h.last_frame().color == -1, "standby stops sending frames");
    m = h.mark();
    h.play("B");
    std::this_thread::sleep_for(300ms);
    expect(h.mark() == m, "standby ignores track requests");

    h.send("producer:active");
    m = h.mark();
    h.play("A");
    expect(h.wait_message("track:ready:A", m), "active producer plays again");
    expect(h.wait_color(color_of("A")), "matrix shows A again");
}

void stop_during_swap()
{
    std::cout << "-- stop and destruction racing a swap\n";
    for (int i = 0; i < 5; ++i) {
        Harness h(/*crossfade_ms=*/200);
        auto m = h.mark();
        h.play("A");
        expect(h.wait_message("track:ready:A", m), "A becomes ready (round " + std::to_string(i) + ")");
        h.play("B");
        std::this_thread::sleep_for(std::chrono::milliseconds(i * 15));
        const auto started = std::chrono::steady_clock::now();
        h.send("stop");
        expect(std::chrono::steady_clock::now() - started < 3s, "stop returns while B is being promoted");
        h.play("C");
        std::this_thread::sleep_for(std::chrono::milliseconds(i * 20));
    } // ~Harness destroys the plugin mid-flight
    expect(true, "no deadlock or crash while stopping/destroying during swaps");
}

} // namespace
#endif

int main()
{
#ifdef _WIN32
    return 0;
#else
    root = fs::temp_directory_path() / ("led-matrix-spotify-flow-" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "home");
    write_fake_tools();

    const char* old_path = std::getenv("PATH");
    const std::string path = (root / "bin").string() + ":" + (old_path ? old_path : "");
    setenv("PATH", path.c_str(), 1);
    setenv("HOME", (root / "home").c_str(), 1);
    setenv("FAKE_MV_ROOT", root.c_str(), 1);

    first_track_and_skip();
    return_to_current_while_loading();
    rapid_skips();
    search_failure_and_retry();
    stop_and_cached_replay();
    producer_handover();
    stop_during_swap();

    fs::remove_all(root);
    if (failures) {
        std::cerr << failures << " SpotifyMV flow check(s) failed\n";
        return 1;
    }
    std::cout << "SpotifyMV flows OK\n";
    return 0;
#endif
}
