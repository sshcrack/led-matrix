#include "state.h"

#include "shared/matrix/input_ids.h"

SpotifyState::SpotifyState(nlohmann::json state_json) : state_json(std::move(state_json)),track(SpotifyTrack(this->state_json.at("item"))) {
    // Empty constructor
}

std::optional<float> SpotifyState::get_progress(tmillis_t additional_ms) {
    auto duration_opt = this->get_track().get_duration();
    if(!duration_opt.has_value())
        return std::nullopt;

    auto curr = this->get_progress_ms() + additional_ms;

    return std::min(1.0f, (float) curr / (float) duration_opt.value());
}

SpotifyTrack SpotifyState::get_track() {
    return this->track;
}

long SpotifyState::get_progress_ms() {
    long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    long fetched_at = this->state_json.value("timestamp", now);

    long diff = now - fetched_at;
    long progress_ms = this->state_json.value("progress_ms", 0);

    return this->is_playing() ? progress_ms + diff : progress_ms;
}

bool SpotifyState::is_playing() {
    return this->state_json.value("is_playing", false);
}


std::optional<SpotifyState> spotify_state_from_runtime_input(
    const RuntimeInputs::Snapshot& snapshot,
    std::chrono::system_clock::time_point now)
{
    if (!snapshot.available(RuntimeInputIds::SpotifyPlayback))
        return std::nullopt;

    const bool playing = snapshot.boolean(RuntimeInputIds::SpotifyPlayback, "playing").value_or(false);
    const auto track_id = snapshot.text(RuntimeInputIds::SpotifyPlayback, "track_id");
    const auto duration = snapshot.number(RuntimeInputIds::SpotifyPlayback, "duration_ms");
    if (!track_id.has_value() || !duration.has_value())
        return std::nullopt;

    const auto progress = snapshot.number(RuntimeInputIds::SpotifyPlayback, "progress_ms").value_or(0.0);
    const auto song = snapshot.text(RuntimeInputIds::SpotifyPlayback, "track").value_or(std::string{});
    const auto artist = snapshot.text(RuntimeInputIds::SpotifyPlayback, "artist").value_or(std::string{});
    const auto cover = snapshot.text(RuntimeInputIds::SpotifyPlayback, "cover_url").value_or(std::string{});
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    const auto* playback = snapshot.find(RuntimeInputIds::SpotifyPlayback);
    const auto input_age_ms = static_cast<long long>(std::llround(
        std::max(0.0, playback != nullptr ? playback->age_seconds : 0.0) * 1000.0));
    const auto source_timestamp_ms = now_ms - input_age_ms;

    nlohmann::json item{
        {"id", *track_id},
        {"name", song},
        {"duration_ms", static_cast<long>(std::max(0.0, *duration))},
        {"artists", nlohmann::json::array({{{"name", artist}}})},
        {"album", {{"images", nlohmann::json::array({{{"url", cover}}})}}},
    };
    return SpotifyState({
        {"timestamp", source_timestamp_ms},
        {"progress_ms", static_cast<long>(std::max(0.0, progress))},
        {"is_playing", playing},
        {"item", std::move(item)},
    });
}
