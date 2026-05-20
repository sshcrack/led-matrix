//
// Created by hendrik on 3/9/25.
//

#include "song_bpm_getter.h"
#include "shared/matrix/utils/shared.h"
#include "spdlog/spdlog.h"
#include "cpr/cpr.h"
#include <expected>
#include "cpr/util.h"

using cpr::util::urlEncode;

std::expected<float, string> SongBpmApi::get_bpm(const std::string& song_name, const std::string& artist_name)
{
    if (!config->get_plugin_configs().contains("song_bpm_api_key"))
    {
        return std::unexpected("No API key found for SongBPM API (must be defined in pluginConfigs.song_bpm_api_key)");
    }

    cpr::Response response = cpr::Get(cpr::Url{"https://api.getsong.co/search/"}, cpr::Parameters{
                                          {"type", "both"},
                                          {"limit", "1"},
                                          {"lookup", "song:" + song_name + " " + "artist:" + artist_name},
                                          {"api_key", config->get_plugin_configs().find("song_bpm_api_key")->second}
                                      });
    if (response.status_code != 200)
    {
        // Returning default BPM
        return std::unexpected("Could not fetch song BPM: " + response.text);
    }

    try
    {
        const json j = nlohmann::json::parse(response.text);
        if (!j.contains("search") || !j["search"].is_array())
            return unexpected("Couldn't find song (most probably):" + response.text);

        const std::vector<json>& search = j["search"];
        if (search.empty() || !search[0].contains("tempo") || !(search[0]["tempo"].is_string()))
            return unexpected("Invalid JSON returned: " + j.dump());

        std::string s = search[0]["tempo"];
        spdlog::trace("Got BPM {}", s);

        return std::stof(s);
    }
    catch (const std::exception& e)
    {
        return unexpected("Could not parse song BPM response: " + string(e.what()));
    }
}
