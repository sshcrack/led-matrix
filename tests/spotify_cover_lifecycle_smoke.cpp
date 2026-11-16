#include "plugins/SpotifyScenes/scenes/CoverOnlyScene.h"
#include "plugins/SpotifyScenes/matrix/manager/shared_spotify.h"
#include "plugins/SpotifyScenes/matrix/manager/song_bpm_getter.h"
#include <shared/matrix/input_ids.h>
#include <shared/matrix/runtime_inputs.h>
#include <emulator.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;
namespace {
std::atomic<int> bpm_started{0}, cancelled{0};
std::atomic<bool> release_bpm{false};
}
// Only the external Spotify/BPM sources are substituted. Scene rendering,
// worker futures, image composition and RuntimeInputs are production code.
Spotify *spotify = nullptr;
std::optional<SpotifyState> Spotify::get_currently_playing() { return std::nullopt; }
bool Spotify::has_changed(bool) { return false; }
std::string Spotify::song_bpm_api_key() const
{
    return {};
}
std::expected<float, std::string> SongBpmApi::get_bpm(const std::string&, const std::string&, const std::string&, std::stop_token stop)
{
    ++bpm_started;
    while (!release_bpm && !stop.stop_requested()) std::this_thread::sleep_for(1ms);
    if (stop.stop_requested()) { ++cancelled; return std::unexpected("cancelled"); }
    return 120.0f;
}

#define CHECK(c) do { if (!(c)) { std::cerr << "Failed line " << __LINE__ << ": " << #c << '\n'; return 1; } } while (0)
int main(int, char** argv)
{
    Magick::InitializeMagick(argv[0]);
    rgb_matrix::RGBMatrix::Options options; options.rows = options.cols = 64;
    rgb_matrix::EmulatorOptions emulator; emulator.headless = true;
    std::unique_ptr<rgb_matrix::EmulatorMatrix> matrix(rgb_matrix::EmulatorMatrix::Create(options, emulator));
    CHECK(matrix);
    auto *canvas = matrix->CreateFrameCanvas();
    const auto prefix = "lifecycle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    struct Cleanup {
        std::vector<std::filesystem::path> files;
        ~Cleanup() { for (auto &file : files) { std::error_code ec; std::filesystem::remove(file, ec); } RuntimeInputs::clear_all(); }
    } cleanup;
    for (const auto &[suffix, color] : std::vector<std::pair<std::string, std::string>>{{"a","red"},{"b","green"},{"c","blue"},{"d","yellow"}}) {
        const auto path = std::filesystem::temp_directory_path() / ("spotify_cover." + prefix + suffix + ".jpg");
        Magick::Image(Magick::Geometry(64,64), Magick::Color(color)).write(path.string());
        cleanup.files.push_back(path);
    }
    auto publish = [&](const std::string &suffix, bool playing = true) {
        RuntimeInputs::publish(RuntimeInputIds::SpotifyPlayback, {
            {"playing", playing}, {"track_id", prefix + suffix}, {"track", std::string("held BPM")},
            {"artist", std::string("fixture")}, {"cover_url", std::string("unused://local-fixture")},
            {"progress_ms", std::int64_t{1000}}, {"duration_ms", std::int64_t{30000}}
        });
    };
    auto scene = std::make_unique<Scenes::CoverOnlyScene>();
    scene->update_default_properties(); scene->register_properties();
    scene->load_properties({{"show_progress",false},{"spectrum_aura",false},{"beat_sparkles",false},
        {"background_blur",0},{"cover_border_glow_intensity",0.0},{"beat_pulse_strength",0.0}});
    scene->initialize(64,64);
    auto tick = [&] { return scene->render_frame(canvas, 1.0/60.0, true); };
    auto until = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        do { if (!tick()) return false; if (predicate()) return true; std::this_thread::sleep_for(1ms); }
        while (std::chrono::steady_clock::now() < deadline);
        return false;
    };
    auto color_is = [&](int channel) {
        uint8_t r=0,g=0,b=0;
        canvas->GetPixel(32,32,&r,&g,&b);
        const int channels[]={r,g,b};
        return channels[channel] > 150 && channels[channel] > channels[(channel+1)%3] * 2
            && channels[channel] > channels[(channel+2)%3] * 2;
    };
    publish("a");
    CHECK(until([&]{return bpm_started >= 1 && color_is(0);})); // Cover appears while BPM is still blocked.
    CHECK(!release_bpm);
    publish("a",false);
    CHECK(tick() && !scene->frame_was_updated()); // Paused media does not swap an untouched buffer.
    publish("b");
    CHECK(until([&]{return bpm_started >= 2;}));
    publish("c"); // Supersede B before its final BPM/animation result is ready.
    CHECK(until([&]{return bpm_started >= 3 && color_is(2);}));
    release_bpm=true;
    CHECK(tick());
    CHECK(color_is(2)); // A stale A/B completion must never replace C.
    // Let C's completed result be collected before starting another blocked task.
    CHECK(until([&]{return cancelled >= 2 && color_is(2);}));
    release_bpm=false;
    publish("d");
    CHECK(until([&]{return bpm_started >= 4;}));
    const auto before = std::chrono::steady_clock::now();
    scene.reset();
    CHECK(std::chrono::steady_clock::now() - before < 1s);
    CHECK(cancelled >= 3);
    std::cout << "Artwork remains responsive during stalled BPM, pause, rapid track replacement and teardown\n";
}
