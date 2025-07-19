#pragma once

#include <shared/common/utils/utils.h>
#include <shared/matrix/Scene.h>

#include "image_providers/general.h"
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;


namespace ConfigData {
    struct Preset {
        vector<std::shared_ptr<Scenes::Scene> > scenes;


        static std::shared_ptr<Preset> create_default();
        ~Preset() = default; // Add explicit destructor
    };

    struct SpotifyData {
        optional<string> access_token;
        optional<string> refresh_token;
        tmillis_t expires_at;

        [[nodiscard]] bool is_expired() const;

        [[nodiscard]] bool has_auth() const;
    };

    struct Schedule {
        string id;
        string name;
        string preset_id;
        int start_hour;     // 0-23
        int start_minute;   // 0-59
        int end_hour;       // 0-23
        int end_minute;     // 0-59
        vector<int> days_of_week; // 0-6 (Sunday=0, Monday=1, ..., Saturday=6)
        bool enabled;
        
        [[nodiscard]] bool is_active_now() const;
        [[nodiscard]] bool is_active_at_time(int hour, int minute, int day_of_week) const;
    };

    struct Root {
        map<string, std::shared_ptr<Preset>> presets;
        map<string, string> pluginConfigs;
        SpotifyData spotify;
        map<string, Schedule> schedules;
        bool scheduling_enabled;
        string curr;

        ~Root() = default;
    };

    void to_json(json &j, const Schedule &p);

    void to_json(json &j, const Root &p);

    void to_json(json &j, std::shared_ptr<Preset> p);

    void to_json(json &j, const SpotifyData &p);

    void to_json(json &j, const ImageProviders::General *&p);

    void to_json(json &j, const Scenes::Scene *&p);

    void from_json(const json &j, Schedule &p);

    void from_json(const json &j, Root &p);

    void from_json(const json &j, std::shared_ptr<Preset> &p);

    void from_json(const json &j, SpotifyData &p);

    void from_json(const json &j, ImageProviders::General *&p);

    void from_json(const json &j, Scenes::Scene *&p);
}
