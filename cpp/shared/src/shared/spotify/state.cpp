#include "state.h"

SpotifyState::SpotifyState(nlohmann::json state_json) : state_json(std::move(state_json)),track(SpotifyTrack(this->state_json.at("item"))) {
    // Empty constructor
}

float SpotifyState::get_progress() {
    auto duration = this->get_track().get_duration();
    auto curr = this->get_progress_ms();

    return (float) curr / (float) duration;
}

SpotifyTrack SpotifyState::get_track() {
    return this->track;
}

long SpotifyState::get_progress_ms() {
    long fetched_at = this->state_json["timestamp"].template get<long>();
    long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    long diff = now - fetched_at;

    return this->state_json["progress_ms"].template get<long>() + diff;
}
