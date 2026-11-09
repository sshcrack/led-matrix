#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include <fstream>
#include "data.h"
#include "shared/matrix/plugin/main.h"

using json = nlohmann::json;

namespace Config {
    class MainConfig {
        mutable std::shared_mutex data_mutex;
        ConfigData::Root data;
        std::atomic<std::uint64_t> automatic_director_generation_{0};
        std::atomic<std::uint64_t> automatic_preferences_version_{0};

        std::shared_mutex update_mutex;
        bool dirty;
        const std::string file_name;

        void load_from_file();
        void migrate_presets();
    public:
        explicit MainConfig(std::string filename);

        void mark_dirty();
        bool is_dirty();

        std::string get_curr_id();
        std::string get_operation_mode();
        bool is_automatic_mode();
        void set_operation_mode(const std::string &mode);
        std::uint64_t get_automatic_director_seed();
        std::uint64_t get_automatic_director_generation() const;
        void set_automatic_director_seed(std::uint64_t seed);
        std::string get_automatic_director_mode();
        /// "mixed", "environment", "party" or "chill"; throws on anything else.
        void set_automatic_director_mode(const std::string &mode);
        std::map<std::string, std::string> get_automatic_preferences();
        /// Increments whenever preferences change so consumers can cache them.
        std::uint64_t get_automatic_preferences_version() const;
        /// `preference` is "favorite", "on", "hidden" or "default" (removes it).
        void set_automatic_preference(const std::string &look_key, const std::string &preference);
        ConfigData::SpotifyData get_spotify();

        std::shared_ptr<ConfigData::Preset> get_curr();

        std::map<std::string, std::shared_ptr<ConfigData::Preset>> get_presets();
        [[nodiscard]] std::map<std::string, std::string> get_plugin_configs() const;

        void set_spotify(ConfigData::SpotifyData spotify);
        void set_curr(std::string id);

        bool delete_preset(const std::string &id);
        bool set_preset_display_name(const std::string& id, const std::string& display_name);

        void set_presets(const std::string& id, std::shared_ptr<ConfigData::Preset> preset);

        void set_plugin_config(const std::string& pluginId, const std::string& config);
        
        bool is_turned_off();
        void set_turned_off(bool turned_off);

        // Schedule management methods
        std::map<std::string, ConfigData::Schedule> get_schedules();
        std::vector<ConfigData::CustomSceneVariant> get_custom_scene_variants(const std::string &scene_name);
        void set_custom_scene_variant(const std::string &scene_name, const ConfigData::CustomSceneVariant &variant);
        bool delete_custom_scene_variant(const std::string &scene_name, const std::string &variant_id);
        void set_schedule(const std::string& id, const ConfigData::Schedule& schedule);
        bool delete_schedule(const std::string& id);
        ConfigData::BrightnessSchedule get_brightness_schedule();
        void set_brightness_schedule(ConfigData::BrightnessSchedule schedule);
        Environment::Settings get_environment();
        void set_environment(Environment::Settings settings);
        /// Caches an IP-detected location without touching the user's settings.
        void set_environment_detected(double latitude, double longitude, std::string label);

        bool is_scheduling_enabled();
        void set_scheduling_enabled(bool enabled);
        std::optional<std::string> get_active_scheduled_preset();
        
        // Update management methods
        ConfigData::UpdateSettings get_update_settings();
        void set_update_settings(const ConfigData::UpdateSettings& settings);
        bool is_auto_update_enabled();
        void set_auto_update_enabled(bool enabled);
        int get_update_check_interval_hours();
        void set_update_check_interval_hours(int hours);
        tmillis_t get_last_check_time();
        void set_last_check_time(tmillis_t time);
        
        bool save();
        /// Release plugin-owned scene instances after configuration has been
        /// persisted and before plugin DSOs are unloaded.
        void release_scene_references();
        std::string get_filename() const;
    };
}