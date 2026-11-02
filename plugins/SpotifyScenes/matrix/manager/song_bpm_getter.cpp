//
// Created by hendrik on 3/9/25.
//

#include "song_bpm_getter.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
#include "cpr/cpr.h"
#include <expected>
#include <chrono>
#include <cmath>
#include "cpr/util.h"

using cpr::util::urlEncode;

std::expected<float, string> SongBpmApi::get_bpm(const std::string &song_name, const std::string &artist_name, std::stop_token stop) {
    if (stop.stop_requested()) return std::unexpected("BPM request cancelled");
    const auto plugin_configs = config->get_plugin_configs();
    const auto api_key = plugin_configs.find("song_bpm_api_key");
    if (api_key == plugin_configs.end()) {
        return std::unexpected("No API key found for SongBPM API (must be defined in pluginConfigs.song_bpm_api_key)");
    }

    const auto str_url = "https://api.getsong.co/search/?type=both&limit=1&lookup=song:" + std::string(urlEncode(song_name)) +
                         "%20artist:" + std::string(urlEncode(artist_name)) + "&api_key=" + api_key->second;
    auto url = cpr::Url{str_url};

    auto response = cpr::Get(url,
        cpr::ConnectTimeout{std::chrono::seconds(5)},
        cpr::Timeout{std::chrono::seconds(20)},
        cpr::LowSpeed{1024, std::chrono::seconds(10)},
        cpr::ProgressCallback{[stop](auto, auto, auto, auto, intptr_t) { return !stop.stop_requested(); }});
    if (response.status_code != 200) {
        // Returning default BPM
        return std::unexpected("Could not fetch song BPM: " + response.text);
    }

    try {
        const json j = nlohmann::json::parse(response.text);
        if (!j.contains("search") || !j["search"].is_array())
            return unexpected("Couldn't find song (most probably):" + response.text);

        const std::vector<json> &search = j["search"];
        if (search.empty() || !search[0].contains("tempo") || !(search[0]["tempo"].is_string()))
            return unexpected("Invalid JSON returned: " + j.dump());

        std::string s = search[0]["tempo"].template get<std::string>();
        spdlog::trace("Got BPM {}", s);

        const float bpm = std::stof(s);
        if (!std::isfinite(bpm) || bpm <= 0.0f)
            return unexpected("Song BPM must be finite and positive");
        return bpm;
    } catch (const std::exception &e) {
        return unexpected("Could not parse song BPM response: " + string(e.what()));
    }

}
