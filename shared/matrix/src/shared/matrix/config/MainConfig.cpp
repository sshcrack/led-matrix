#include "shared/config/MainConfig.h"
#include <mutex>
#include "shared/utils/shared.h"
#include <utility>
#include "spdlog/spdlog.h"
#include <vector>
#include <algorithm>


using namespace spdlog;

namespace Config {
    void MainConfig::mark_dirty() {
        unique_lock lock(this->update_mutex);

        exit_canvas_update = true;
        this->dirty = true;
    }

    bool MainConfig::is_dirty() {
        shared_lock lock(this->update_mutex);
        return this->dirty;
    }

    string MainConfig::get_curr_id() {
        shared_lock lock(this->data_mutex);

        return this->data.curr;
    }

    std::shared_ptr<ConfigData::Preset> MainConfig::get_curr() {
        shared_lock lock(this->data_mutex);

        return this->data.presets[data.curr];
    }

    ConfigData::SpotifyData MainConfig::get_spotify() {
        shared_lock lock(this->data_mutex);

        return this->data.spotify;
    }

    void MainConfig::set_spotify(ConfigData::SpotifyData spotify) {
        unique_lock lock(this->data_mutex);

        this->data.spotify = std::move(spotify);
        this->mark_dirty();
    }

    void MainConfig::set_curr(string id) {
        unique_lock lock(this->data_mutex);

        this->data.curr = std::move(id);
        this->mark_dirty();
    }

    bool MainConfig::delete_preset(const string &id) {
        unique_lock lock(this->data_mutex);

        const auto it = this->data.presets.find(id);
        if (it == this->data.presets.end())
            return false;

        this->data.presets.erase(it);
        return true;
    }

    void MainConfig::set_presets(const string &id, std::shared_ptr<ConfigData::Preset> preset) {
        unique_lock lock(this->data_mutex);

        this->data.presets[id] = std::move(preset);
        this->mark_dirty();
    }

    map<string, std::shared_ptr<ConfigData::Preset>> MainConfig::get_presets() {
        shared_lock lock(this->data_mutex);
        return this->data.presets;
    }

    bool MainConfig::save() {
        try {
            debug("Acquiring lock to save config...");
            shared_lock lock(this->data_mutex);

            info("Saving config at '{}'..", file_name);
            json as_json = this->data;
            string out = as_json.dump(2);

            ofstream file;
            file.open(file_name);

            if (!(file << out)) {
                error("Could not write to file '{}'", file_name);
                return false;
            }

            file.close();
            info("Done saving config.");
        } catch (exception &ex) {
            error("could not save config: {}", ex.what());
            return false;
        }

        return true;
    }

    MainConfig::MainConfig(const string filename) : file_name(filename) {
        if (!filesystem::exists(filename)) {
            debug("Writing default config at '{}'...", filename);
            ofstream file;
            file.open(filename);
            file << "{}";
            file.close();
        }

        ifstream f(filename);
        json temp = json::parse(f);

        f.close();

        this->data = temp.get<ConfigData::Root>();
        this->dirty = false;
    }

    string MainConfig::get_filename() const {
        return this->file_name;
    }

    map<string, string> MainConfig::get_plugin_configs() const {
        return this->data.pluginConfigs;
    }

    void MainConfig::set_plugin_config(const std::string &pluginId, const string &config) {
        unique_lock lock(this->data_mutex);

        this->data.pluginConfigs[pluginId] = config;
        this->mark_dirty();
    }

    map<string, ConfigData::Schedule> MainConfig::get_schedules() {
        shared_lock lock(this->data_mutex);
        return this->data.schedules;
    }

    void MainConfig::set_schedule(const string& id, const ConfigData::Schedule& schedule) {
        unique_lock lock(this->data_mutex);
        this->data.schedules[id] = schedule;
        this->mark_dirty();
    }

    bool MainConfig::delete_schedule(const string& id) {
        unique_lock lock(this->data_mutex);
        const auto it = this->data.schedules.find(id);
        if (it == this->data.schedules.end())
            return false;

        this->data.schedules.erase(it);
        this->mark_dirty();
        return true;
    }

    bool MainConfig::is_scheduling_enabled() {
        shared_lock lock(this->data_mutex);
        return this->data.scheduling_enabled;
    }

    void MainConfig::set_scheduling_enabled(bool enabled) {
        unique_lock lock(this->data_mutex);
        this->data.scheduling_enabled = enabled;
        this->mark_dirty();
    }

    optional<string> MainConfig::get_active_scheduled_preset() {
        shared_lock lock(this->data_mutex);
        
        if (!this->data.scheduling_enabled) {
            return nullopt;
        }

        // Find all active schedules and sort by priority (shorter duration = higher priority)
        vector<pair<string, ConfigData::Schedule>> active_schedules;
        
        for (const auto& [id, schedule] : this->data.schedules) {
            if (schedule.is_active_now()) {
                active_schedules.emplace_back(id, schedule);
            }
        }
        
        if (active_schedules.empty()) {
            return nullopt;
        }
        
        // Sort by duration (ascending), so shorter schedules come first
        sort(active_schedules.begin(), active_schedules.end(), 
             [](const auto& a, const auto& b) {
                 const auto& schedule_a = a.second;
                 const auto& schedule_b = b.second;
                 
                 // Calculate duration in minutes for each schedule
                 int duration_a = (schedule_a.end_hour * 60 + schedule_a.end_minute) - 
                                  (schedule_a.start_hour * 60 + schedule_a.start_minute);
                 int duration_b = (schedule_b.end_hour * 60 + schedule_b.end_minute) - 
                                  (schedule_b.start_hour * 60 + schedule_b.start_minute);
                 
                 // Handle schedules that cross midnight
                 if (duration_a < 0) duration_a += 24 * 60;  // Add 24 hours worth of minutes
                 if (duration_b < 0) duration_b += 24 * 60;
                 
                 return duration_a < duration_b;  // Shorter duration = higher priority
             });
        
        // Return the preset_id of the highest priority (shortest duration) schedule
        return active_schedules[0].second.preset_id;
    }
}
