#pragma once

#include <shared_mutex>
#include "nlohmann/json.hpp"
#include <fstream>
#include "data.h"
#include "shared/matrix/plugin/main.h"

using namespace std;
using json = nlohmann::json;

namespace Config {
    class MainConfig {
        shared_mutex data_mutex;
        ConfigData::Root data;

        shared_mutex update_mutex;
        bool dirty;
        const string file_name;
    public:
        explicit MainConfig(string filename);

        void mark_dirty();
        bool is_dirty();

        string get_curr_id();
        ConfigData::SpotifyData get_spotify();

        std::shared_ptr<ConfigData::Preset> get_curr();

        map<string, std::shared_ptr<ConfigData::Preset>> get_presets();
        [[nodiscard]] map<string, string> get_plugin_configs() const;

        void set_spotify(ConfigData::SpotifyData spotify);
        void set_curr(string id);

        bool delete_preset(const string &id);

        void set_presets(const string& id, std::shared_ptr<ConfigData::Preset> preset);

        void set_plugin_config(const std::string& pluginId, const string& config);
        
        bool is_turned_off();
        void set_turned_off(bool turned_off);

        // Schedule management methods
        map<string, ConfigData::Schedule> get_schedules();
        void set_schedule(const string& id, const ConfigData::Schedule& schedule);
        bool delete_schedule(const string& id);
        bool is_scheduling_enabled();
        void set_scheduling_enabled(bool enabled);
        optional<string> get_active_scheduled_preset();
        
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
        string get_filename() const;
    };
}