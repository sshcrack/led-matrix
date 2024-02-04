#include "MainConfig.h"
#include <mutex>
#include "../shared.h"
#include <utility>
#include <spdlog/spdlog.h>


namespace Config {
    void MainConfig::mark_dirty(bool dirty_local) {
        unique_lock<shared_mutex> lock(this->update_mutex);
        if(dirty_local)
            exit_canvas_update = true;
        this->dirty = dirty_local;
    }

    bool MainConfig::is_dirty() {
        spdlog::debug("Checking if dirty...");
        shared_lock<shared_mutex> lock(this->update_mutex);
        spdlog::debug("Returning");
        return this->dirty;
    }

    string MainConfig::get_curr_id() {
        shared_lock<shared_mutex> lock(this->data_mutex);

        return this->data.curr;
    }

    ConfigData::Group MainConfig::get_curr() {
        shared_lock<shared_mutex> lock(this->data_mutex);

        return this->data.groups[data.curr];
    }

    void MainConfig::setCurr(string id) {
        unique_lock<shared_mutex> lock(this->data_mutex);

        this->data.curr = std::move(id);
        this->mark_dirty(true);
    }

    map<string, ConfigData::Group> MainConfig::get_groups() {
        shared_lock<shared_mutex> lock(this->data_mutex);
        return this->data.groups;
    }
}
