//
// Created by hendrik on 3/9/25.
//

#include "song_bpm_getter.h"
#include <shared/utils/shared.h>
#include <spdlog/spdlog.h>
#include <cpr/cpr.h>
#include <cpr/util.h>

using cpr::util::urlEncode;

float SongBpmApi::get_bpm(const std::string &song_name, const std::string &artist_name) {
    const auto api_key_entry = config->get_plugin_configs().find("song_bpm_api_key");

    if (api_key_entry == config->get_plugin_configs().end()) {
        spdlog::error("No API key found for SongBPM API (must be defined in pluginConfigs.song_bpm_api_key)");

        // Returning default BPM
        return 120.0f;
    }

    const auto str_url = "https://api.getsong.co/search/?type=both&limit=1&lookup=song:" + urlEncode(song_name) +
                         "%20artist:" + urlEncode(artist_name) + "&api_key=" + config->get_plugin_configs().find("song_bpm_api_key")->second;
    auto url = cpr::Url{str_url};

    auto response = cpr::Get(url);
    if (response.status_code != 200) {
        spdlog::error("Could not fetch song BPM: {}", response.text);

        // Returning default BPM
        goto return_default;
    }

    try {
        const json j = nlohmann::json::parse(response.text);
        if (!j.contains("search") || !j["search"].is_array())
            goto return_default;

        const std::vector<json> &search = j["search"];
        if (search.size() == 0 || !search[0].contains("tempo") || !search[0]["tempo"].is_number())
            goto return_default;

        float s = search[0]["tempo"];
        spdlog::trace("Got BPM {}", s);

        return search[0]["tempo"];
    } catch (const std::exception &e) {
        spdlog::error("Could not parse song BPM response: {}", e.what());
    }

return_default:
    return 120.0f;
}
