#include "MainConfig.h"
#include <mutex>
#include "../utils/shared.h"
#include <utility>
#include <spdlog/spdlog.h>


using namespace spdlog;
namespace Config {
    void MainConfig::mark_dirty(bool dirty_local) {
        unique_lock<shared_mutex> lock(this->update_mutex);
        if(dirty_local)
            exit_canvas_update = true;
        this->dirty = dirty_local;
    }

    bool MainConfig::is_dirty() {
        shared_lock<shared_mutex> lock(this->update_mutex);
        return this->dirty;
    }

    string MainConfig::get_curr_id() {
        shared_lock<shared_mutex> lock(this->data_mutex);

        return this->data.curr;
    }

    ConfigData::Preset MainConfig::get_curr() {
        shared_lock<shared_mutex> lock(this->data_mutex);

        return this->data.presets[data.curr];
    }

    void MainConfig::set_curr(string id) {
        unique_lock<shared_mutex> lock(this->data_mutex);

        this->data.curr = std::move(id);
        this->mark_dirty(true);
    }

    void MainConfig::set_presets(const string& id, ConfigData::Preset preset) {
        unique_lock<shared_mutex> lock(this->data_mutex);

        this->data.presets[id] = std::move(preset);
        this->mark_dirty(true);
    }

    map<string, ConfigData::Preset> MainConfig::get_groups() {
        shared_lock<shared_mutex> lock(this->data_mutex);
        return this->data.presets;
    }

    bool MainConfig::save() {
        try {
            debug("Acquiring lock to save config...");
            shared_lock<shared_mutex> lock(this->data_mutex);

            info("Saving config...");
            json as_json = this->data;
            string out = as_json.dump();

            ofstream file;
            file.open(file_name);
            file << out;
            file.close();
            info("Done saving config.");
        } catch (exception& ex) {
            error("could not save config: {}", ex.what());
            return false;
        }

        return true;
    }

    MainConfig::MainConfig(const string &filename) : file_name(filename) {
        if (!filesystem::exists(filename)) {
            ofstream file;
            file.open(filename);
            file << jsonDefault;
            file.close();
        }

        ifstream f(filename);
        json temp = json::parse(f);

        this->data = temp.template get<ConfigData::Root>();
        this->dirty = false;
    }
}
