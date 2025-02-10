#pragma once

#include "shared/config/image_providers/general.h"
#include "Scene.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct Preset {
        vector<std::shared_ptr<Scenes::Scene>> scenes;
        vector<std::shared_ptr<ImageProviders::General>> providers;

    public:
        void randomize();
    };

    struct SpotifyData {
        optional<string> access_token;
        optional<string> refresh_token;
        tmillis_t expires_at;

        [[nodiscard]] bool is_expired() const;
        [[nodiscard]] bool has_auth() const;
    };

    struct Root {
        map<string, Preset> presets;
        map<string, string> pluginConfigs;
        SpotifyData spotify;
        string curr;
    };

    void to_json(json& j, const Root& p);
    void to_json(json& j, const Preset& p);
    void to_json(json& j, const SpotifyData& p);
    void to_json(json& j, const ImageProviders::General*& p);
    void to_json(json& j, const Scenes::Scene*& p);

    void from_json(const json& j, Root& p);
    void from_json(const json& j, Preset& p);
    void from_json(const json& j, SpotifyData& p);
    void from_json(const json& j, ImageProviders::General*& p);
    void from_json(const json& j, Scenes::Scene*& p);
}