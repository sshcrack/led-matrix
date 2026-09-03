#include "plugins/SpotifyScenes/matrix/manager/state.h"

#include <shared/matrix/input_ids.h>

#include <cstdint>
#include <iostream>
#include <string>

int main()
{
    RuntimeInputs::InputState playback;
    playback.available = true;
    playback.age_seconds = 2.0;
    playback.signals = {
        {"playing", true},
        {"track_id", std::string("track-a")},
        {"track", std::string("Song")},
        {"artist", std::string("Artist")},
        {"cover_url", std::string{}},
        {"progress_ms", std::int64_t{1000}},
        {"duration_ms", std::int64_t{10000}},
    };

    RuntimeInputs::Snapshot snapshot({
        {std::string(RuntimeInputIds::SpotifyPlayback), playback},
    });

    auto state = spotify_state_from_runtime_input(snapshot);
    if (!state.has_value()) {
        std::cerr << "runtime Spotify state was not created\n";
        return 1;
    }

    // The Runtime Input was published two seconds ago at progress=1s. While
    // playback is active, CoverOnly must therefore render roughly 3s progress
    // even if no newer Spotify packet has arrived yet.
    const long progress_ms = state->get_progress_ms();
    if (progress_ms < 2800 || progress_ms > 3400) {
        std::cerr << "runtime Spotify progress did not interpolate from input age: "
                  << progress_ms << " ms\n";
        return 2;
    }

    playback.signals["playing"] = false;
    RuntimeInputs::Snapshot paused_snapshot({
        {std::string(RuntimeInputIds::SpotifyPlayback), playback},
    });
    auto paused_state = spotify_state_from_runtime_input(paused_snapshot);
    if (!paused_state.has_value() || paused_state->get_progress_ms() != 1000) {
        std::cerr << "paused Spotify progress incorrectly continued advancing\n";
        return 3;
    }

    std::cout << "runtime Spotify progress interpolates between producer updates\n";
    return 0;
}
