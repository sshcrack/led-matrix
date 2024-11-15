#pragma once

#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include "data.h"
#include "shared/resources.h"
#include "plugin.h"

using namespace std;
using json = nlohmann::json;

namespace Config {
    class MainConfig {
    private:
        shared_mutex data_mutex;
        ConfigData::Root data;

        shared_mutex update_mutex;
        bool dirty;
        const string file_name;
    public:
        explicit MainConfig(string filename);

        void mark_dirty(bool dirty_local);
        bool is_dirty();

        string get_curr_id();
        ConfigData::SpotifyData get_spotify();
        ConfigData::Preset get_curr();
        map<string, ConfigData::Preset> get_presets();
        [[nodiscard]] map<string, string> get_plugin_configs() const;

        void set_spotify(ConfigData::SpotifyData spotify);
        void set_curr(string id);
        void set_presets(const string& id, ConfigData::Preset preset);

        void set_plugin_config(const std::string& pluginId, const string& config);
        bool save();
        string get_filename();

    };
}