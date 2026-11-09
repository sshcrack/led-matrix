#pragma once

#include <shared/common/utils/utils.h>
#include <shared/matrix/Scene.h>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "image_providers/general.h"
#include "nlohmann/json.hpp"
#include <shared/matrix/environment/settings.h>

using json = nlohmann::json;

namespace ConfigData
{
    struct Preset
    {
        std::vector<std::shared_ptr<Scenes::Scene>> scenes;
        tmillis_t transition_duration = 750;
        std::string transition_name = "blend";  ///< Global default transition effect name
        std::string display_name;

        static std::shared_ptr<Preset> create_default();
        ~Preset() = default; // Add explicit destructor
    };

    struct CustomSceneVariant
    {
        std::string id;
        std::string label;
        std::string description;
        json properties = json::object();
    };

    struct SpotifyData
    {
        std::optional<std::string> access_token;
        std::optional<std::string> refresh_token;
        tmillis_t expires_at = 0;
        std::string client_id;
        std::string client_secret; // legacy; logins started after PKCE support use no secret
        bool pkce = false;    // tokens were issued via PKCE: refresh with client_id only

        [[nodiscard]] bool is_expired() const;

        [[nodiscard]] bool has_auth() const;
    };

    struct Schedule
    {
        std::string id;
        std::string name;
        std::string preset_id;
        int start_hour;           // 0-23
        int start_minute;         // 0-59
        int end_hour;             // 0-23
        int end_minute;           // 0-59
        std::vector<int> days_of_week; // 0-6 (Sunday=0, Monday=1, ..., Saturday=6)
        bool enabled;

        [[nodiscard]] bool is_active_now() const;
        [[nodiscard]] bool is_active_at_time(int hour, int minute, int day_of_week) const;
    };

    /// Dims the panel during a nightly window, fading linearly over fade_minutes after each boundary.
    struct BrightnessSchedule
    {
        bool enabled = false;
        int day_brightness = 100;   // 1-100 (percent)
        int night_brightness = 25;  // 1-100 (percent)
        int night_start_hour = 22;
        int night_start_minute = 0;
        int night_end_hour = 7;
        int night_end_minute = 0;
        int fade_minutes = 20;      // 0-180

        /// Brightness in percent for a wall-clock time given as minutes since midnight (0-1439).
        [[nodiscard]] int brightness_at(int minute_of_day) const;
        /// Clamps all fields into their valid ranges.
        void sanitize();
    };

    struct UpdateSettings
    {
        bool auto_update_enabled = true;
        int check_interval_hours = 24; // Check for updates every 24 hours by default
        tmillis_t last_check_time = 0;
        bool update_available = false;
        std::string latest_version = "";
        std::string update_download_url = "";
    };

    struct Root
    {
        std::map<std::string, std::shared_ptr<Preset>> presets;
        std::map<std::string, std::string> pluginConfigs;
        SpotifyData spotify;
        std::map<std::string, Schedule> schedules;
        std::map<std::string, std::vector<CustomSceneVariant>> custom_scene_variants;
        bool scheduling_enabled;
        BrightnessSchedule brightness_schedule;
        std::atomic<bool> turned_off;
        std::string operation_mode = "automatic";
        std::uint64_t automatic_director_seed = 0;
        /// "mixed" | "environment" | "party" | "chill" (what Automatic Mode is for).
        std::string automatic_director_mode = "mixed";
        /// Look key ("scene" or "scene/variant") -> "favorite" | "on" | "hidden".
        /// Absent keys follow the look's automatic_default.
        std::map<std::string, std::string> automatic_preferences;
        std::string curr;
        UpdateSettings update_settings;
        Environment::Settings environment;

        // Custom move assignment operator to handle atomic<bool>
        Root &operator=(Root &&other) noexcept
        {
            if (this != &other)
            {
                presets = std::move(other.presets);
                pluginConfigs = std::move(other.pluginConfigs);
                spotify = std::move(other.spotify);
                schedules = std::move(other.schedules);
                custom_scene_variants = std::move(other.custom_scene_variants);
                scheduling_enabled = other.scheduling_enabled;
                brightness_schedule = other.brightness_schedule;
                turned_off.store(other.turned_off.load());
                operation_mode = std::move(other.operation_mode);
                automatic_director_seed = other.automatic_director_seed;
                automatic_director_mode = std::move(other.automatic_director_mode);
                automatic_preferences = std::move(other.automatic_preferences);
                curr = std::move(other.curr);
                update_settings = std::move(other.update_settings);
                environment = std::move(other.environment);
            }
            return *this;
        }

        // Custom move constructor to handle atomic<bool>
        Root(Root &&other) noexcept
            : presets(std::move(other.presets)),
              pluginConfigs(std::move(other.pluginConfigs)),
              spotify(std::move(other.spotify)),
              schedules(std::move(other.schedules)),
              custom_scene_variants(std::move(other.custom_scene_variants)),
              scheduling_enabled(other.scheduling_enabled),
              brightness_schedule(other.brightness_schedule),
              turned_off(other.turned_off.load()),
              operation_mode(std::move(other.operation_mode)),
              automatic_director_seed(other.automatic_director_seed),
              automatic_director_mode(std::move(other.automatic_director_mode)),
              automatic_preferences(std::move(other.automatic_preferences)),
              curr(std::move(other.curr)),
              update_settings(std::move(other.update_settings)),
              environment(std::move(other.environment))
        {
        }

        // Delete copy constructor and copy assignment operator
        Root(const Root &) = delete;
        Root &operator=(const Root &) = delete;

        // Default constructor
        Root() = default;

        ~Root() = default;
    };

    void to_json(json &j, const CustomSceneVariant &p);
    void from_json(const json &j, CustomSceneVariant &p);

    void to_json(json &j, const UpdateSettings &p);

    void to_json(json &j, const Schedule &p);

    void to_json(json &j, const BrightnessSchedule &p);
    void from_json(const json &j, BrightnessSchedule &p);

    void to_json(json &j, const Root &p);

    void to_json(json &j, std::shared_ptr<Preset> p);

    void to_json(json &j, const SpotifyData &p);

    void to_json(json &j, const ImageProviders::General *&p);

    void to_json(json &j, const Scenes::Scene *&p);

    void from_json(const json &j, UpdateSettings &p);

    void from_json(const json &j, Schedule &p);

    void from_json(const json &j, Root &p);

    void from_json(const json &j, std::shared_ptr<Preset> &p);

    void from_json(const json &j, SpotifyData &p);

}
