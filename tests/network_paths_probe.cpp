// Live-network probe for the daemon's credential-free outbound paths. It needs internet
// and talks to real services, so ctest skips it unless LED_MATRIX_NETWORK_TESTS=1.
// Run it from a sanitizer build to check the success paths, not just the error paths:
//
//   source scripts/sanitizers/env.sh
//   LED_MATRIX_NETWORK_TESTS=1 ctest --test-dir emulator_asan_build -L network --output-on-failure
//
// LED_MATRIX_NETWORK_TESTS=no-download skips the Pi .deb download.
//
// Covers: Open-Meteo geocoding and forecast, the Environment service's IP location +
// weather loop, the Deezer genre lookup (GenreLookup without Spotify credentials), and the
// Pi updater's GitHub release query, .deb download and SHA-256 check. The update is never
// installed: the UpdateManager is not started, so stop() does not launch the update script.
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "plugins/SpotifyScenes/matrix/manager/genre_lookup.h"
#include "shared/matrix/environment/service.h"
#include "shared/matrix/input_ids.h"
#include "shared/matrix/runtime_inputs.h"
#include "shared/matrix/update/UpdateManager.h"
#include "shared/matrix/utils/shared.h"

namespace {
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS: " : "FAIL: ") << what << '\n';
    if (!ok)
        ++failures;
}

bool wait_for(std::chrono::seconds timeout, const std::function<bool()>& done)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (done())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return done();
}

void probe_open_meteo()
{
    const auto places = Environment::geocode("Berlin");
    check(places.has_value() && places->is_array() && !places->empty(),
          "Open-Meteo geocode(\"Berlin\") " + (places ? places->dump().substr(0, 80) : places.error()));
    if (!places || places->empty())
        return;
    const double lat = (*places)[0].value("latitude", 0.0);
    const double lon = (*places)[0].value("longitude", 0.0);
    const auto weather = Environment::fetch_weather(lat, lon);
    check(weather.has_value(), "Open-Meteo fetch_weather " + (weather ? std::string("ok") : weather.error()));
}

void probe_environment_service()
{
    Environment::Settings settings;  // enabled, auto_locate, no manual or cached location
    bool persisted = false;
    auto& service = Environment::Service::instance();
    service.start(settings, [&](const Environment::Settings&) { persisted = true; });
    nlohmann::json status;
    const bool ok = wait_for(std::chrono::seconds(30), [&] {
        status = service.status();
        const auto& location = status["location"];
        const auto& conditions = status["conditions"];
        return location.is_object() && location.value("source", "") == "ip" && conditions.is_object() &&
               conditions.contains("temperature_c") && !conditions["temperature_c"].is_null();
    });
    service.stop();
    check(ok, "Environment::Service located by IP and fetched weather (location source " +
                  (status["location"].is_object() ? status["location"].value("source", std::string{"none"}) : std::string{"none"}) + ")");
    check(persisted, "Environment::Service persisted the IP-detected location");
}

void probe_deezer_genres()
{
    auto& lookup = GenreLookup::instance();
    // No Spotify fetch set: the lookup goes straight to Deezer.
    lookup.track_playing("network-probe-track", "Smells Like Teen Spirit", "Nirvana");
    std::string genres, source;
    const bool ok = wait_for(std::chrono::seconds(30), [&] {
        const auto snapshot = RuntimeInputs::snapshot();
        const auto* state = snapshot.find(RuntimeInputIds::MusicGenre);
        if (!state || !state->available)
            return false;
        if (const auto it = state->signals.find("genres"); it != state->signals.end())
            if (const auto* text = std::get_if<std::string>(&it->second))
                genres = *text;
        if (const auto it = state->signals.find("source"); it != state->signals.end())
            if (const auto* text = std::get_if<std::string>(&it->second))
                source = *text;
        return !genres.empty();
    });
    lookup.stop();
    check(ok && source.find("deezer") != std::string::npos, "Deezer genre lookup published \"" + genres + "\" (source " + source + ")");
}

void probe_update_download(Config::MainConfig& main_config, bool download)
{
    Update::UpdateManager updates(&main_config);
    const auto info = updates.get_update_info();
    check(info.has_value(), "GitHub latest release " + (info ? info->version.toString() + " " + info->download_url : info.error()));
    if (!info || !download)
        return;
    check(!info->sha256.empty(), "release asset publishes a SHA-256 digest");
    // Downloads into the updater's private directory and verifies the digest; marks the
    // update pending, which only runs the installer from stop() of a started manager.
    const auto result = updates.manual_download_and_install(*info);
    check(result.has_value(), "download + SHA-256 verify " + (result ? std::string("ok") : result.error()));
}
}  // namespace

int main()
{
    const char* enabled = std::getenv("LED_MATRIX_NETWORK_TESTS");
    if (!enabled || std::string(enabled).empty() || std::string(enabled) == "0") {
        std::cout << "network_paths_probe: skipped (set LED_MATRIX_NETWORK_TESTS=1)\n";
        return 77;
    }
    const bool download = std::string(enabled) != "no-download";
    const auto work = std::filesystem::temp_directory_path() / ("led-matrix-network-probe-" + std::to_string(::getpid()));
    std::filesystem::create_directories(work);
    // GenreLookup caches under ./cache; the updater downloads below TMPDIR when
    // /var/lib/led-matrix/update is not writable. Keep both inside `work`.
    std::filesystem::current_path(work);
    ::setenv("TMPDIR", work.c_str(), 1);
    // The daemon sets the global config before any plugin code runs; GenreLookup reads it.
    auto main_config = std::make_unique<Config::MainConfig>((work / "config.json").string());
    config = main_config.get();

    probe_open_meteo();
    probe_environment_service();
    probe_deezer_genres();
    probe_update_download(*main_config, download);
    config = nullptr;
    main_config.reset();

    std::error_code ec;
    std::filesystem::remove_all(work, ec);
    std::cout << (failures ? "network_paths_probe: FAILED\n" : "network_paths_probe: all passed\n");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
