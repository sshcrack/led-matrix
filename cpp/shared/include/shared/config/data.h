#pragma once

#include "config/image_providers/general.h"
#include <nlohmann/json.hpp>
#include "shared/utils/utils.h"
using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct Preset {
        string name;
        vector<ImageProviders::General*> categories;

    public:
        void randomize();
    };

    struct SpotifyData {
        optional<string> access_token;
        optional<string> refresh_token;
        tmillis_t expires_at;

        bool is_expired() const;
        bool has_auth() const;
    };

    struct Root {
        map<string, Preset> presets;
        SpotifyData spotify;
        string curr;
    };

    void to_json(json& j, const Root& p);
    void to_json(json& j, const Preset& p);
    void to_json(json& j, const SpotifyData& p);
    void to_json(json& j, const ImageProviders::General*& p);

    void from_json(const json& j, Root& p);
    void from_json(const json& j, Preset& p);
    void from_json(const json& j, SpotifyData& p);
    void from_json(const json& j, ImageProviders::General*& p);
}