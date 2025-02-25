#include "shared/config/MainConfig.h"
#include <mutex>
#include "shared/utils/shared.h"
#include <utility>
#include <spdlog/spdlog.h>
#include "shared/resources.h"


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
}
