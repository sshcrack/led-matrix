#include "state.h"

SpotifyState::SpotifyState(nlohmann::json state_json) : state_json(std::move(state_json)),track(SpotifyTrack(this->state_json.at("item"))) {
    // Empty constructor
}

float SpotifyState::get_progress(tmillis_t additional_ms) {
    auto duration = this->get_track().get_duration();
    auto curr = this->get_progress_ms() + additional_ms;

    return std::min(1.0f, (float) curr / (float) duration);
}

SpotifyTrack SpotifyState::get_track() {
    return this->track;
}

long SpotifyState::get_progress_ms() {
    long fetched_at = this->state_json["timestamp"].template get<long>();
    long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    long diff = now - fetched_at;
    long progress_ms = this->state_json["progress_ms"].template get<long>();

    return this->is_playing() ? progress_ms + diff : progress_ms;
}

bool SpotifyState::is_playing() {
    return this->state_json["is_playing"].template get<bool>();
}
