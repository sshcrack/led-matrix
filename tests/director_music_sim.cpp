// Offline Automatic Director simulation against recorded music analysis.
//
// Loads the real plugin catalog (like preview_gen), replays an .audr file
// produced by tests/audio_analysis_dump through AudioState / Runtime Inputs and
// drives AutomaticDirector exactly like CanvasCoordinator does: choose ->
// presentation_duration -> consider_switch every tick -> handoff. Prints the
// resulting timeline so director behaviour can be judged against real songs.
//
//   director_music_sim <song.audr> [--desktop] [--seed N] [--genre "Electro,Dance"]
#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>

#include "matrix_control/AutomaticDirector.h"
#include "shared/common/audio_protocol.h"
#include "shared/matrix/audio_state.h"
#include "shared/matrix/canvas_consts.h"
#include "shared/matrix/config/MainConfig.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/music_style.h"
#include "shared/matrix/plugin_loader/loader.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/utils/consts.h"
#include "shared/matrix/utils/shared.h"

namespace fs = std::filesystem;

static std::vector<AudioProtocol::Frame> load_audr(const std::string& path, int& fps)
{
    std::vector<AudioProtocol::Frame> frames;
    std::ifstream in(path, std::ios::binary);
    char magic[4]{};
    uint32_t f = 0, count = 0;
    in.read(magic, 4);
    in.read(reinterpret_cast<char*>(&f), 4);
    in.read(reinterpret_cast<char*>(&count), 4);
    if (!in || std::memcmp(magic, "AUDR", 4) != 0)
        return frames;
    fps = static_cast<int>(f);
    std::vector<uint8_t> bytes;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t len = 0;
        if (!in.read(reinterpret_cast<char*>(&len), 4) || len > 64 * 1024)
            break;
        bytes.resize(len);
        if (!in.read(reinterpret_cast<char*>(bytes.data()), len))
            break;
        AudioProtocol::Frame frame;
        if (AudioProtocol::decode(bytes, frame))
            frames.push_back(std::move(frame));
    }
    return frames;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: director_music_sim song.audr [--desktop] [--seed N] [--genre \"a,b\"] [--title T --artist A]\n";
        return 2;
    }
    spdlog::set_level(spdlog::level::warn);
    bool desktop = false, spotify = false;
    uint64_t seed = 7;
    std::string genre, title = "Simulated", artist = "Simulated";
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--desktop")
            desktop = true;
        else if (a == "--spotify")
            spotify = true;
        else if (a == "--seed" && i + 1 < argc)
            seed = std::stoull(argv[++i]);
        else if (a == "--genre" && i + 1 < argc)
            genre = argv[++i];
        else if (a == "--title" && i + 1 < argc)
            title = argv[++i];
        else if (a == "--artist" && i + 1 < argc)
            artist = argv[++i];
    }

    int fps = 30;
    const auto frames = load_audr(argv[1], fps);
    if (frames.empty()) {
        std::cerr << "could not read " << argv[1] << '\n';
        return 1;
    }

    Constants::width = 128;
    Constants::height = 128;
    const fs::path cfg_path = fs::temp_directory_path() / ("director_sim-" + std::to_string(std::random_device{}()) + ".json");
    config = new Config::MainConfig(cfg_path.string());
    const auto pl = Plugins::PluginManager::instance();
    pl->initialize();

    std::vector<std::shared_ptr<Scenes::Scene>> catalog;
    for (const auto& wrapper : pl->get_scenes()) {
        if (!wrapper)
            continue;
        const auto descriptor = wrapper->get_default()->get_descriptor();
        if (!descriptor.automatic_eligible)
            continue;
        auto make = [&](const std::string& variant) {
            auto s = wrapper->create();
            s->update_default_properties();
            s->register_properties();
            if (!variant.empty())
                s->apply_variant(variant);
            else
                s->load_properties(nlohmann::json::object());
            catalog.push_back(std::shared_ptr<Scenes::Scene>(std::move(s)));
        };
        if (descriptor.variants.empty())
            make("");
        else
            for (const auto& v : descriptor.variants) make(v.id);
    }

    AutomaticDirector director(seed);
    constexpr int TickMs = 200;
    const int framesPerTick = std::max(1, fps * TickMs / 1000);
    const auto t0 = std::chrono::steady_clock::now();
    size_t fed = 0;
    auto publish = [&](size_t index) {
        // Feed every analysis frame on the audio timeline (as the UDP stream would).
        index = std::min(index, frames.size() - 1);
        for (; fed <= index; ++fed)
            AudioState::update(frames[fed], t0 + std::chrono::microseconds(static_cast<int64_t>(fed * 1e6 / fps)));
        if (desktop)
            RuntimeInputs::set_available(RuntimeInputIds::Desktop, true, {{"connected", true}});
        if (!genre.empty())
            RuntimeInputs::publish(RuntimeInputIds::MusicGenre,
                                   {{"track_id", std::string("sim")}, {"genres", genre}, {"source", std::string("sim")}},
                                   std::chrono::seconds(30));
        if (spotify)
            RuntimeInputs::publish(RuntimeInputIds::SpotifyPlayback,
                               {{"playing", true}, {"track_id", std::string("sim")}, {"track", title}, {"artist", artist},
                                {"progress_ms", static_cast<double>(index) * 1000.0 / fps},
                                {"duration_ms", static_cast<double>(frames.size()) * 1000.0 / fps}},
                               std::chrono::seconds(30));
    };

    size_t frame = 0;
    publish(0);
    std::map<std::string, double> screen_time;
    std::string exclude;
    std::shared_ptr<Scenes::Scene> preferred;
    int presentations = 0;
    while (frame < frames.size()) {
        publish(frame);
        auto decision = director.choose(catalog, RuntimeInputs::snapshot(), exclude, preferred);
        if (!decision.scene)
            break;
        const auto scene = decision.scene;
        const auto planned = director.presentation_duration(scene, RuntimeInputs::snapshot());
        director.record_played(scene);
        const double start = static_cast<double>(frame) / fps;
        std::string reason = "end of track";
        tmillis_t elapsed = 0;
        preferred = nullptr;
        while (frame < frames.size()) {
            frame += static_cast<size_t>(framesPerTick);
            elapsed += TickMs;
            director.advance_journey(TickMs);
            publish(frame);
            const auto op = director.consider_switch(catalog, scene, RuntimeInputs::snapshot(), elapsed, planned);
            if (op.should_switch) {
                reason = op.reason;
                preferred = op.preferred_scene;
                break;
            }
            if (elapsed >= planned + AutomaticDirector::phrase_grace_ms) {
                reason = "hard deadline";
                break;
            }
        }
        director.report_outcome(scene, {});
        const double end = std::min(static_cast<double>(frame), static_cast<double>(frames.size())) / fps;
        const auto key = AutomaticDirector::look_key(*scene);
        screen_time[key] += end - start;
        ++presentations;
        const auto style = MusicStyle::describe(RuntimeInputs::snapshot());
        std::printf("%6.1fs %-34s planned %4.1fs shown %5.1fs  [%s %.0f%% %s] -> %s\n", start, key.c_str(), planned / 1000.0,
                    end - start, std::string(MusicStyle::name(style.dominant)).c_str(),
                    style.weights[static_cast<size_t>(style.dominant)] * 100.0f, style.source.c_str(), reason.c_str());
        if (std::getenv("SIM_TOP")) {
            for (size_t i = 0; i < std::min<size_t>(5, decision.ranked.size()); ++i)
                std::printf("          #%zu %-36s %.2f\n", i + 1, AutomaticDirector::look_key(*decision.ranked[i].scene).c_str(),
                            decision.ranked[i].score);
        }
        exclude = scene->get_name();
    }
    std::printf("\n%d presentations over %.0fs (mean %.1fs)\n", presentations, frames.size() / static_cast<double>(fps),
                frames.size() / static_cast<double>(fps) / std::max(1, presentations));
    std::vector<std::pair<double, std::string>> sorted;
    for (const auto& [k, v] : screen_time) sorted.emplace_back(v, k);
    std::sort(sorted.rbegin(), sorted.rend());
    for (const auto& [v, k] : sorted) std::printf("  %5.1fs  %s\n", v, k.c_str());

    catalog.clear();
    pl->delete_references();
    pl->destroy_plugins();
    std::error_code ignored;
    fs::remove(cfg_path, ignored);
    std::_Exit(0);
}
